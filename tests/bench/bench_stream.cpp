#include <QDebug>
#include <QString>
#include <QObject>
#include <QVector>
#include <QIODevice>
#include <QThread>
#include <QtTest>

#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

#include <array>
#include <memory>

#include "lbu/eventfd.h"
#include "lbu/fd_stream.h"
#include "lbu/io.h"
#include "lbu/pipe.h"
#include "lbu/poll.h"
#include "lbu/ring_spsc.h"
#include "lbu/ring_spsc_stream.h"


static const unsigned s_chunk_size = 16; // modify to test different chunk sizes

static const unsigned s_transfer_size = ((1024*1024*1024) / sizeof(int) / s_chunk_size) * s_chunk_size;
static inline int valueForIndex(unsigned i)
{
    return ((i % 2) == 0) ? 1 : -1;
}
static const int s_expected = 0;


static const unsigned s_chunk_byte_size = s_chunk_size * sizeof(int);
static const unsigned s_transfer_byte_size = s_transfer_size * sizeof(int);

static int s_result = 0;

static std::array<int, s_chunk_size> s_read_buffer;
static std::array<int, 1024*16> s_ring_buffer;
static const unsigned s_ring_chunk_size = s_chunk_size;//s_ring_buffer.size() / 4;
static std::array<int, s_chunk_size> s_write_buffer;
static std::atomic<size_t> s_producerIdx;
static std::atomic<size_t> s_consumerIdx;
static bool s_wake_request = false;


// ---------- QIODevice test class

class QtFdDev : public QIODevice {
    Q_OBJECT

public:
    QtFdDev(int fd) : m_fd(fd) {}
    ~QtFdDev() { m_fd.close(); }

    bool isSequential() const { return true; }

protected:
    qint64 readData(char *data, qint64 maxlen)
    {
        auto a = lbu::array_ref<char>(data, maxlen < 0 ? 0 : size_t(maxlen));
//        ssize_t res;
//        if(lbu::io::read(m_fd, a, &res) != lbu::io::ReadNoError)
//            return -1;
//        return res;
        return (lbu::io::read_all(m_fd, a) == lbu::io::ReadNoError) ? qint64(a.size()) : -1;
    }

    qint64 writeData(const char *data, qint64 len)
    {
        auto a = lbu::array_ref<const char>(data, len < 0 ? 0 : size_t(len));
        return (lbu::io::write_all(m_fd, a) == lbu::io::WriteNoError) ? len : -1;
    }

private:
    lbu::fd m_fd;
};


// ---------- Signal & Slot test classes

class QtSigSlotTransferSender : public QObject {
    Q_OBJECT

public:
    QtSigSlotTransferSender()
    {
        d.resize(s_chunk_size);
    }

    void run()
    {
        for( unsigned i = 0; i < (s_transfer_size / s_chunk_size); ++i ) {
            for( unsigned j = 0; j < s_chunk_size; ++j ) {
                d[int(j)] = valueForIndex(j + (i * s_chunk_size));
            }
            emit dataAvailable(d);
        }
        emit done();
    }

signals:
    void dataAvailable(const QVector<int> &data);
    void done();

private:
    QVector<int> d;
};

class QtSigSlotTransferReceiver : public QObject {
    Q_OBJECT

public slots:
    void process(const QVector<int> &data)
    {
        for( auto f : data )
            s_result += f;
    }

    void done() { QThread::currentThread()->quit(); }
};

Q_DECLARE_METATYPE(QVector<int>)


// ---------- Bench reader threads

class Thread_RawIO : public QThread {
    Q_OBJECT
    void run() override
    {
        unsigned processed = 0;
        char *rawBuffer = reinterpret_cast<char*>(s_read_buffer.data());

        size_t offset = 0;
        while( processed < s_transfer_size ) {
            const ssize_t bytes = read(m_fd, rawBuffer + offset, s_chunk_byte_size - offset);
            Q_ASSERT(bytes > 0);

            const unsigned count = unsigned(bytes) / sizeof(int);
            for( unsigned i = 0; i < count; ++i )
                s_result += s_read_buffer[i];

            processed += count;

            offset = unsigned(bytes) % sizeof(int);
            if( offset != 0 ) {
                qDebug() << "OFFSET" << offset;
                memmove(rawBuffer, rawBuffer + (count * sizeof(int)), offset);
            }
        }
    }
    int m_fd = 0;
public:
    Thread_RawIO(int fd)
    {
        m_fd = fd;
        Q_ASSERT(m_fd);
    }
    ~Thread_RawIO()
    {
        close(m_fd);
    }
};

class Thread_FILE : public QThread {
    Q_OBJECT
    void run() override
    {
        unsigned processed = 0;
        while( processed < s_transfer_size ) {
            const int c = fread_unlocked(s_read_buffer.data(), sizeof(int), s_chunk_size, m_file);
            Q_ASSERT(c > 0);

            const unsigned count = unsigned(c);
            for( size_t i = 0; i < count; ++i )
                s_result += s_read_buffer[i];

            processed += count;
        }
    }
    FILE* m_file = nullptr;
public:
    Thread_FILE(int fd)
    {
        m_file = fdopen(fd, "r");
        Q_ASSERT(m_file);
    }
    ~Thread_FILE()
    {
        fclose(m_file);
    }
};

class Thread_QIODev : public QThread {
    Q_OBJECT
    void run() override
    {
        char *rawBuffer = reinterpret_cast<char*>(s_read_buffer.data());
        unsigned processed = 0;
        while( processed < s_transfer_size ) {
            const int c = m_dev.read(rawBuffer, s_chunk_byte_size);
            Q_ASSERT(c == s_chunk_byte_size);

            for( unsigned i = 0; i < s_chunk_size; ++i )
                s_result += s_read_buffer[i];

            processed += s_chunk_size;
        }
    }
    QtFdDev m_dev;
public:
    Thread_QIODev(int fd)
        : m_dev(fd)
    {
        //auto b = m_dev.open(QIODevice::ReadOnly);
        auto b = m_dev.open(QIODevice::ReadOnly|QIODevice::Unbuffered);
        Q_ASSERT(b);
    }
};

class Thread_FdStream : public QThread {
    Q_OBJECT
    void run() override
    {
        unsigned processed = 0;
        auto *s = m_stream.stream();

        while( processed < s_transfer_size ) {
            const int c = s->read(s_read_buffer.data(), s_chunk_byte_size, lbu::stream::Mode::Blocking);
            Q_ASSERT(c == s_chunk_byte_size);

            for( unsigned i = 0; i < s_chunk_size; ++i )
                s_result += s_read_buffer[i];

            processed += s_chunk_size;
        }
    }
    lbu::stream::managed_fd_input_stream m_stream;
public:
    Thread_FdStream(int fd)
    {
        m_stream.reset(lbu::unique_fd(fd), lbu::stream::FdBlockingState::Blocking);
    }
};

class Thread_RingStream : public QThread {
    Q_OBJECT
    void run() override
    {
        unsigned processed = 0;

        while( processed < s_transfer_size ) {
            const int c = in.read(s_read_buffer.data(), s_chunk_byte_size, lbu::stream::Mode::Blocking);
            Q_ASSERT(c == s_chunk_byte_size);

            for( unsigned i = 0; i < s_chunk_size; ++i )
                s_result += s_read_buffer[i];

            processed += s_chunk_size;
        }
    }
public:
    lbu::stream::ring_spsc::input_stream in;
};

#ifdef BENCH_RING_ADDITIONAL
class Thread_RingSpin : public QThread {
    Q_OBJECT
    void run() override
    {
        unsigned processed = 0;

        while( processed < s_transfer_size ) {
            auto r = consumer.continuous_range();
            if( r.size() == 0 ) {
                while( consumer.update_available() == 0 )
                    yieldCurrentThread();
                r = consumer.continuous_range();
            }
            r = r.sub_first(s_ring_chunk_size);

            for( auto v : r )
                s_result += v;

            processed += r.size();
            consumer.release(r.size());
        }
    }
public:
    lbu::ring_spsc::handle<int>::consumer consumer;
};

class Thread_RingBlockFd : public QThread {
    Q_OBJECT
    void run() override
    {
        unsigned processed = 0;
        const lbu::fd f(m_poll.fd);
        eventfd_t unused;

        while( processed < s_transfer_size ) {
            auto r = consumer.continuous_range();
            if( r.size() == 0 ) {
                if( consumer.update_available() == 0 ) {
                    int c = 0;
                    do {
                        lbu::poll::poll(&m_poll, 1, &c);
                    } while( c < 1 );
                    consumer.update_available();
                }
                r = consumer.continuous_range();
            }
            r = r.sub_first(s_ring_chunk_size);

            for( auto v : r )
                s_result += v;

            processed += r.size();
            consumer.release(r.size());
            lbu::event_fd::read(f, &unused);
        }
    }
    pollfd m_poll;
public:
    Thread_RingBlockFd(lbu::fd f)
        : m_poll(lbu::poll::poll_fd(f, lbu::poll::FlagsReadReady))
    {
    }
    lbu::ring_spsc::handle<int>::consumer consumer;
};

class Thread_RingBlockCond : public QThread {
    Q_OBJECT
    void run() override
    {
        unsigned processed = 0;

        while( processed < s_transfer_size ) {
            auto r = consumer.continuous_range();
            if( r.size() == 0 ) {
                if( consumer.update_available() == 0 ) {
                    pthread_mutex_lock(m_mutex);
                    while( consumer.update_available() == 0 ) {
                        s_wake_request = true;
                        pthread_cond_wait(m_cond, m_mutex);
                    }
                    pthread_mutex_unlock(m_mutex);
                }
                r = consumer.continuous_range();
            }
            r = r.sub_first(s_ring_chunk_size);

            for( auto v : r )
                s_result += v;

            processed += r.size();
            consumer.release(r.size());

            bool sendSignal = false;
            pthread_mutex_lock(m_mutex);
            sendSignal = s_wake_request;
            s_wake_request = false;
            pthread_mutex_unlock(m_mutex);
            if( sendSignal )
                pthread_cond_signal(m_cond);
        }
    }
    pthread_mutex_t* m_mutex;
    pthread_cond_t* m_cond;
public:
    Thread_RingBlockCond(pthread_mutex_t* mutex, pthread_cond_t* cond)
        : m_mutex(mutex)
        , m_cond(cond)
    {
    }
    lbu::ring_spsc::handle<int>::consumer consumer;
};
#endif


// ---------- Main

class BenchStream : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void RawIO()
    {
        lbu::fd readFd = {}, writeFd = {};
        QVERIFY(lbu::pipe::open(&readFd, &writeFd) == lbu::pipe::StatusNoError);
        const auto buf = lbu::array_ref<int>(s_write_buffer);

        Thread_RawIO t(readFd.value);
        QBENCHMARK {
            s_result = 0;
            t.start();

            const unsigned iter = s_transfer_size / s_chunk_size;
            for( unsigned i = 0; i < iter; ++i ) {
                for( unsigned j = 0; j < s_chunk_size; ++j ) {
                    s_write_buffer[j] = valueForIndex(i*s_chunk_size + j);
                }
                if( lbu::io::write_all(writeFd, buf) != lbu::io::WriteNoError )
                    Q_ASSERT(false);
            }

            t.wait();
        }

        writeFd.close();
        QCOMPARE(s_result, s_expected);
    }

    void FILE_io()
    {
        lbu::fd readFd = {}, writeFd = {};
        QVERIFY(lbu::pipe::open(&readFd, &writeFd) == lbu::pipe::StatusNoError);
        FILE* pipe_out = fdopen(writeFd.value, "w");
        QVERIFY(pipe_out);

        Thread_FILE t(readFd.value);
        QBENCHMARK {
            s_result = 0;
            t.start();

            const unsigned iter = s_transfer_size / s_chunk_size;
            for( unsigned i = 0; i < iter; ++i ) {
                for( unsigned j = 0; j < s_chunk_size; ++j ) {
                    s_write_buffer[j] = valueForIndex(i*s_chunk_size + j);
                }
                if( fwrite_unlocked(s_write_buffer.data(), sizeof(int), s_chunk_size, pipe_out) != s_chunk_size )
                    Q_ASSERT(false);
            }

            fflush(pipe_out);
            t.wait();
        }

        fclose(pipe_out);
        QCOMPARE(s_result, s_expected);
    }

    void QIODev()
    {
        lbu::fd readFd = {}, writeFd = {};
        QVERIFY(lbu::pipe::open(&readFd, &writeFd) == lbu::pipe::StatusNoError);

        QtFdDev writeDev(writeFd.value);
        QVERIFY(writeDev.open((QIODevice::WriteOnly)));
        Thread_QIODev t(readFd.value);

        QBENCHMARK {
            s_result = 0;
            t.start();

            const unsigned iter = s_transfer_size / s_chunk_size;
            for( unsigned i = 0; i < iter; ++i ) {
                for( unsigned j = 0; j < s_chunk_size; ++j ) {
                    s_write_buffer[j] = valueForIndex(i*s_chunk_size + j);
                }
                if( writeDev.write(reinterpret_cast<char*>(s_write_buffer.data()), s_chunk_byte_size) != s_chunk_byte_size )
                    Q_ASSERT(false);
            }

            if( writeDev.bytesToWrite() != 0 )
                Q_ASSERT(false);
            t.wait();
        }

        QCOMPARE(s_result, s_expected);
    }

    void QSigSlot()
    {
        qRegisterMetaType<QVector<int>>();
        QThread t;
        QtSigSlotTransferSender sender;
        QtSigSlotTransferReceiver receiver;
        sender.connect(&sender, &QtSigSlotTransferSender::dataAvailable, &receiver, &QtSigSlotTransferReceiver::process);
        sender.connect(&sender, &QtSigSlotTransferSender::done, &receiver, &QtSigSlotTransferReceiver::done);
        receiver.moveToThread(&t);

        QBENCHMARK {
            s_result = 0;
            t.start();
            sender.run();
            t.wait();
        }

        QCOMPARE(s_result, s_expected);
    }

    void FdStream()
    {
        lbu::fd readFd = {}, writeFd = {};
        QVERIFY(lbu::pipe::open(&readFd, &writeFd) == lbu::pipe::StatusNoError);

        lbu::stream::managed_fd_output_stream writeDev;
        writeDev.reset(lbu::unique_fd(writeFd), lbu::stream::FdBlockingState::Blocking);
        auto *s = writeDev.stream();
        std::unique_ptr<Thread_FdStream> t(new Thread_FdStream(readFd.value));

        QBENCHMARK {
            s_result = 0;
            t->start();

            const unsigned iter = s_transfer_size / s_chunk_size;
            for( unsigned i = 0; i < iter; ++i ) {
                for( unsigned j = 0; j < s_chunk_size; ++j ) {
                    s_write_buffer[j] = valueForIndex(i*s_chunk_size + j);
                }
                auto r = s->write(s_write_buffer.data(), s_chunk_byte_size, lbu::stream::Mode::Blocking);
                if( r != ssize_t(s_chunk_byte_size) )
                    Q_ASSERT(false);
            }

            if( ! s->flush_buffer() )
                Q_ASSERT(false);
            t->wait();
        }

        QCOMPARE(s->has_error(), false);
        QCOMPARE(s_result, s_expected);
    }

    void RingStream()
    {
        //lbu::stream::ring_spsc_basic_controller controller(s_ring_buffer.size() * sizeof(int));
        lbu::stream::ring_spsc_basic_controller controller;
        lbu::stream::ring_spsc::output_stream out;
        std::unique_ptr<Thread_RingStream> t(new Thread_RingStream);
        QVERIFY(controller.pair_streams(&out, &t->in));

        QBENCHMARK {
            s_result = 0;
            t->start();

            const unsigned iter = s_transfer_size / s_chunk_size;
            for( unsigned i = 0; i < iter; ++i ) {
                for( unsigned j = 0; j < s_chunk_size; ++j ) {
                    s_write_buffer[j] = valueForIndex(i*s_chunk_size + j);
                }
                auto r = out.write(s_write_buffer.data(), s_chunk_byte_size, lbu::stream::Mode::Blocking);
                if( r != ssize_t(s_chunk_byte_size) )
                    Q_ASSERT(false);
            }

            if( ! out.flush_buffer() )
                Q_ASSERT(false);
            t->wait();
        }

        QCOMPARE(out.has_error(), false);
        QCOMPARE(s_result, s_expected);
    }

#ifdef BENCH_RING_ADDITIONAL
    void RingSpin()
    {
        s_producerIdx = s_consumerIdx = 0;
        std::unique_ptr<Thread_RingSpin> t(new Thread_RingSpin);
        lbu::ring_spsc::handle<int>::producer producer;

        lbu::ring_spsc::handle<int>::pair_producer_consumer({s_ring_buffer.begin(), s_ring_buffer.end()},
                                                            &producer, &s_producerIdx,
                                                            &t->consumer, &s_consumerIdx);

        QBENCHMARK {
            s_result = 0;
            t->start();

            unsigned processed = 0;

            while( true ) {
                auto c = s_transfer_size - processed;
                if( c == 0 )
                    break;

                auto r = producer.continuous_range();
                if( r.size() == 0 ) {
                    while( producer.update_available() == 0 )
                        QThread::yieldCurrentThread();
                    r = producer.continuous_range();
                }
                r = r.sub_first(std::min(s_ring_chunk_size, c));

                for( unsigned i = 0; i < r.size(); ++i )
                    r[i] = 1;

                processed += r.size();
                producer.publish(r.size());
            }

            t->wait();
        }

        QCOMPARE(s_result, int(s_transfer_size));
    }

    void RingBlockFd()
    {
        s_producerIdx = s_consumerIdx = 0;
        lbu::fd f;
        QVERIFY(lbu::event_fd::open(&f, 0, lbu::event_fd::FlagsNonBlock) == lbu::event_fd::OpenNoError);
        lbu::poll::unique_pollfd e(f, lbu::poll::FlagsWriteReady);

        std::unique_ptr<Thread_RingBlockFd> t(new Thread_RingBlockFd(e.descriptor()));
        lbu::ring_spsc::handle<int>::producer producer;

        lbu::ring_spsc::handle<int>::pair_producer_consumer({s_ring_buffer.begin(), s_ring_buffer.end()},
                                                            &producer, &s_producerIdx,
                                                            &t->consumer, &s_consumerIdx);

        QBENCHMARK {
            s_result = 0;
            t->start();

            unsigned processed = 0;

            while( true ) {
                auto c = s_transfer_size - processed;
                if( c == 0 )
                    break;

                auto r = producer.continuous_range();
                if( r.size() == 0 ) {
                    if( producer.update_available() == 0 ) {
                        int c = 0;
                        do {
                            lbu::poll::poll(e.as_pollfd(), 1, &c);
                        } while( c < 1 );
                        producer.update_available();
                    }
                    r = producer.continuous_range();
                }
                r = r.sub_first(std::min(s_ring_chunk_size, c));

                for( unsigned i = 0; i < r.size(); ++i )
                    r[i] = 1;

                processed += r.size();
                producer.publish(r.size());
                lbu::event_fd::write(e.descriptor(), lbu::event_fd::MaximumValue);
            }

            t->wait();
        }

        QCOMPARE(s_result, int(s_transfer_size));
    }


    void RingBlockCond()
    {
        s_producerIdx = s_consumerIdx = 0;

        pthread_mutex_t mutex;
        pthread_cond_t cond;

        pthread_mutex_init(&mutex, nullptr);
        pthread_cond_init(&cond, nullptr);

        std::unique_ptr<Thread_RingBlockCond> t(new Thread_RingBlockCond(&mutex, &cond));
        lbu::ring_spsc::handle<int>::producer producer;

        lbu::ring_spsc::handle<int>::pair_producer_consumer({s_ring_buffer.begin(), s_ring_buffer.end()},
                                                            &producer, &s_producerIdx,
                                                            &t->consumer, &s_consumerIdx);

        QBENCHMARK {
            s_result = 0;
            t->start();

            unsigned processed = 0;

            while( true ) {
                auto c = s_transfer_size - processed;
                if( c == 0 )
                    break;

                auto r = producer.continuous_range();
                if( r.size() == 0 ) {
                    if( producer.update_available() == 0 ) {
                        pthread_mutex_lock(&mutex);
                        while( producer.update_available() == 0 ) {
                            s_wake_request = true;
                            pthread_cond_wait(&cond, &mutex);
                        }
                        pthread_mutex_unlock(&mutex);
                    }
                    r = producer.continuous_range();
                }
                r = r.sub_first(std::min(s_ring_chunk_size, c));

                for( unsigned i = 0; i < r.size(); ++i )
                    r[i] = 1;

                processed += r.size();
                producer.publish(r.size());

                bool sendSignal = false;
                pthread_mutex_lock(&mutex);
                sendSignal = s_wake_request;
                s_wake_request = false;
                pthread_mutex_unlock(&mutex);
                if( sendSignal )
                    pthread_cond_signal(&cond);
            }

            t->wait();
        }

        QCOMPARE(s_result, int(s_transfer_size));
    }
#endif

};


QTEST_MAIN(BenchStream)

#include "bench_stream.moc"
