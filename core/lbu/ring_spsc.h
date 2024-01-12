/* Copyright 2015-2020 Zeno Sebastian Endemann <zeno.endemann@mailbox.org>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LIBLBU_RING_SPSC_H
#define LIBLBU_RING_SPSC_H

#include "lbu/array_ref.h"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <limits>
#include <utility>

namespace lbu {
namespace ring_spsc {

namespace algorithm {

    // Implementation of the 2 well known circular buffer algorithms that use a
    // producer and consumer index (which is ideal for the consumer and producer
    // running concurrently) for distinguishing a full from an empty buffer:
    // - reserved_slot: the producer cannot publish the last free slot to the
    //                  consumer until the consumer consumes more slots (thus
    //                  making it not the last anymore)
    // - mirrored_index: basically one bit of the indices is used to mark if the
    //                   buffer is full or not

    template< class SizeType >
    SizeType continuous_slots(SizeType offset, SizeType count, SizeType n)
    {
        return std::min(count, n - offset);
    }

    template< class SizeType >
    class reserved_slot {
    public:
        static bool producer_has_free_slots(SizeType producer_index, SizeType consumer_index, SizeType n)
        {
            assert(n > 1);
            const auto p = producer_index;
            const auto c = consumer_index;

            return (c == 0 ? (p != n-1) : (p != c-1));
        }

        static SizeType producer_free_slots(SizeType producer_index, SizeType consumer_index, SizeType n)
        {
            assert(n > 1);
            const auto p = producer_index;
            const auto c = consumer_index;

            return (p >= c ? (n - p + c) : (c - p)) - 1;
        }

        static bool consumer_has_free_slots(SizeType producer_index, SizeType consumer_index, SizeType)
        {
            const auto p = producer_index;
            const auto c = consumer_index;

            return p != c;
        }

        static SizeType consumer_free_slots(SizeType producer_index, SizeType consumer_index, SizeType n)
        {
            assert(n > 1);
            const auto p = producer_index;
            const auto c = consumer_index;

            return (c <= p ? (p - c) : (n - c + p));
        }

        static SizeType offset(SizeType idx, SizeType)
        {
            return idx;
        }

        static SizeType new_index(SizeType idx, SizeType cnt, SizeType n)
        {
            assert(n > 1);
            assert(cnt < n && cnt >= 0);
            assert(idx < n);
            return ((n - idx > cnt) ? (idx + cnt) : (cnt - (n - idx)));
        }

        static SizeType new_index_increment(SizeType idx, SizeType n)
        {
            assert(n > 1);
            const auto i = idx + 1;
            assert(i <= n);
            return (i == n ? 0 : i);
        }

        static size_t max_size()
        {
            return std::numeric_limits<SizeType>::max();
        }
    };


    template< class SizeType >
    class mirrored_index {
    public:
        static bool producer_has_free_slots(SizeType producer_index, SizeType consumer_index, SizeType n)
        {
            assert(n >= 1 && n <= max_size());
            const auto p = producer_index;
            const auto c = consumer_index;

            return (p > c ? (p - c) : (c - p)) != n;
        }

        static SizeType producer_free_slots(SizeType producer_index, SizeType consumer_index, SizeType n)
        {
            assert(n >= 1 && n <= max_size());
            const auto p = producer_index;
            const auto c = consumer_index;

            return (p >= c ? (n - (p - c)) : (c - n - p));
        }


        static bool consumer_has_free_slots(SizeType producer_index, SizeType consumer_index, SizeType)
        {
            const auto p = producer_index;
            const auto c = consumer_index;

            return p != c;
        }

        static SizeType consumer_free_slots(SizeType producer_index, SizeType consumer_index, SizeType n)
        {
            assert(n >= 1 && n <= max_size());
            const auto p = producer_index;
            const auto c = consumer_index;

            return (c <= p ? (p - c) : (2*n - c + p));
        }

        static SizeType offset(SizeType idx, SizeType n)
        {
            assert(n >= 1 && n <= max_size());
            assert(idx < 2*n);
            return idx >= n ? idx - n : idx;
        }

        static SizeType new_index(SizeType idx, SizeType cnt, SizeType n)
        {
            assert(n >= 1 && n <= max_size());
            assert(cnt <= n && cnt >= 0);
            assert(idx < 2*n);
            return ((2*n - idx > cnt) ? (idx + cnt) : (idx - n + cnt - n));
        }

        static SizeType new_index_increment(SizeType idx, SizeType n)
        {
            assert(n >= 1 && n <= max_size());
            const auto i = idx + 1;
            assert(i <= 2*n);
            return (i == 2*n ? 0 : i);
        }

        static size_t max_size()
        {
            return std::numeric_limits<SizeType>::max() / 2;
        }
    };

    template< class T, class SizeType >
    std::pair<array_ref<T>, array_ref<T>> ranges(T* begin, SizeType offset, SizeType available, SizeType n)
    {
        const auto first = algorithm::continuous_slots(offset, available, n);
        return { {begin + offset, first}, {begin, available - first} };
    }

}


    template< class T, class SizeType = size_t, class BufferAlg = algorithm::mirrored_index<SizeType> >
    class handle {
    private:
        struct data {
            array_ref<T, SizeType> buffer;
            SizeType local_producer_index = 0;
            SizeType local_consumer_index = 0;
            std::atomic<SizeType>* shared_producer_index = {};
            std::atomic<SizeType>* shared_consumer_index = {};

            void reset(array_ref<T, SizeType> buf,
                       std::atomic<SizeType>* p,
                       std::atomic<SizeType>* c)
            {
                buffer = buf;
                shared_producer_index = p;
                shared_consumer_index = c;
                local_producer_index = p->load();
                local_consumer_index = c->load();
            }
        };

    public:

        using shared_index = std::atomic<SizeType>;

        class producer {
        public:
            producer() = default;
            producer(array_ref<T, SizeType> buf,
                     shared_index* producer_index,
                     shared_index* consumer_index);

            void reset(array_ref<T, SizeType> buf,
                       shared_index* producer_index,
                       shared_index* consumer_index);

            SizeType update_available();
            void publish(SizeType count);

            SizeType last_available() const;
            array_ref<T, SizeType> continuous_range();
            std::pair<array_ref<T, SizeType>, array_ref<T, SizeType>> ranges();

            array_ref<T, SizeType> buffer() const { return d.buffer; }

            producer(const producer&) = delete;
            producer& operator=(const producer&) = delete;

        private:
            data d;
        };

        class consumer {
        public:
            consumer() = default;
            consumer(array_ref<T, SizeType> buf,
                     shared_index* producer_index,
                     shared_index* consumer_index);

            void reset(array_ref<T, SizeType> buf,
                       shared_index* producer_index,
                       shared_index* consumer_index);

            SizeType update_available();
            void release(SizeType count);

            SizeType last_available() const;
            array_ref<T, SizeType> continuous_range();
            std::pair<array_ref<T, SizeType>, array_ref<T, SizeType>> ranges();

            array_ref<T, SizeType> buffer() const { return d.buffer; }

            consumer(const consumer&) = delete;
            consumer& operator=(const consumer&) = delete;

        private:
            data d;
        };

        static void pair_producer_consumer(array_ref<T, SizeType> buffer,
                                           producer* p, shared_index* producer_index,
                                           consumer* c, shared_index* consumer_index);
    };

    // implementation

    template< class T, class SizeType, class BufferAlg >
    inline handle<T, SizeType, BufferAlg>::producer::producer(array_ref<T, SizeType> buf,
                                                              shared_index* producer_index,
                                                              shared_index* consumer_index)
    {
        d.reset(buf, producer_index, consumer_index);
    }

    template< class T, class SizeType, class BufferAlg >
    inline void handle<T, SizeType, BufferAlg>::producer::reset(array_ref<T, SizeType> buf,
                                                                shared_index* producer_index,
                                                                shared_index* consumer_index)
    {
        d.reset(buf, producer_index, consumer_index);
    }

    template< class T, class SizeType, class BufferAlg >
    inline SizeType handle<T, SizeType, BufferAlg>::producer::update_available()
    {
        d.local_consumer_index = d.shared_consumer_index->load(std::memory_order_acquire);
        return last_available();
    }

    template< class T, class SizeType, class BufferAlg >
    inline void handle<T, SizeType, BufferAlg>::producer::publish(SizeType count)
    {
        const auto i = BufferAlg::new_index(d.local_producer_index, count, d.buffer.size());
        d.local_producer_index = i;
        d.shared_producer_index->store(i, std::memory_order_release);
    }

    template< class T, class SizeType, class BufferAlg >
    inline SizeType handle<T, SizeType, BufferAlg>::producer::last_available() const
    {
        return BufferAlg::producer_free_slots(d.local_producer_index, d.local_consumer_index, d.buffer.size());
    }

    template< class T, class SizeType, class BufferAlg >
    inline array_ref<T, SizeType> handle<T, SizeType, BufferAlg>::producer::continuous_range()
    {
        const auto off = BufferAlg::offset(d.local_producer_index, d.buffer.size());
        return {d.buffer.begin() + off, algorithm::continuous_slots(off, last_available(), d.buffer.size())};
    }

    template< class T, class SizeType, class BufferAlg >
    inline std::pair<array_ref<T, SizeType>, array_ref<T, SizeType>> handle<T, SizeType, BufferAlg>::producer::ranges()
    {
        const auto n = d.buffer.size();
        return algorithm::ranges(d.buffer.begin(),
                                 BufferAlg::offset(d.local_producer_index, n),
                                 last_available(), n);
    }


    template< class T, class SizeType, class BufferAlg >
    inline handle<T, SizeType, BufferAlg>::consumer::consumer(array_ref<T, SizeType> buf,
                                                              shared_index* producer_index,
                                                              shared_index* consumer_index)
    {
        d.reset(buf, producer_index, consumer_index);
    }

    template< class T, class SizeType, class BufferAlg >
    inline void handle<T, SizeType, BufferAlg>::consumer::reset(array_ref<T, SizeType> buf,
                                                                shared_index* producer_index,
                                                                shared_index* consumer_index)
    {
        d.reset(buf, producer_index, consumer_index);
    }

    template< class T, class SizeType, class BufferAlg >
    inline SizeType handle<T, SizeType, BufferAlg>::consumer::update_available()
    {
        d.local_producer_index = d.shared_producer_index->load(std::memory_order_acquire);
        return last_available();
    }

    template< class T, class SizeType, class BufferAlg >
    inline void handle<T, SizeType, BufferAlg>::consumer::release(SizeType count)
    {
        const auto i = BufferAlg::new_index(d.local_consumer_index, count, d.buffer.size());
        d.local_consumer_index = i;
        d.shared_consumer_index->store(i, std::memory_order_release);
    }

    template< class T, class SizeType, class BufferAlg >
    inline SizeType handle<T, SizeType, BufferAlg>::consumer::last_available() const
    {
        return BufferAlg::consumer_free_slots(d.local_producer_index, d.local_consumer_index, d.buffer.size());
    }

    template< class T, class SizeType, class BufferAlg >
    inline array_ref<T, SizeType> handle<T, SizeType, BufferAlg>::consumer::continuous_range()
    {
        const auto off = BufferAlg::offset(d.local_consumer_index, d.buffer.size());
        return {d.buffer.begin() + off, algorithm::continuous_slots(off, last_available(), d.buffer.size())};
    }

    template< class T, class SizeType, class BufferAlg >
    inline std::pair<array_ref<T, SizeType>, array_ref<T, SizeType>> handle<T, SizeType, BufferAlg>::consumer::ranges()
    {
        const auto n = d.buffer.size();
        return algorithm::ranges(d.buffer.begin(),
                                 BufferAlg::offset(d.local_consumer_index, n),
                                 last_available(), n);
    }


    template< class T, class SizeType, class BufferAlg >
    inline void handle<T, SizeType, BufferAlg>::pair_producer_consumer(array_ref<T, SizeType> buffer,
                                                                       handle<T, SizeType, BufferAlg>::producer* p,
                                                                       std::atomic<SizeType>* producer_index,
                                                                       handle<T, SizeType, BufferAlg>::consumer* c,
                                                                       std::atomic<SizeType>* consumer_index)
    {
        p->reset(buffer, producer_index, consumer_index);
        c->reset(buffer, producer_index, consumer_index);
    }

}
}

#endif
