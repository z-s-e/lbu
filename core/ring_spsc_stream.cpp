/* Copyright 2015-2017 Zeno Sebastian Endemann <zeno.endemann@googlemail.com>
 *
 * This file is part of the lbu library.
 *
 * lbu is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the licence, or (at your option) any later version.
 *
 * lbu is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 */

#include "lbu/ring_spsc_stream.h"

#include "lbu/align.h"
#include "lbu/eventfd.h"
#include "lbu/poll.h"
#include "lbu/ring_spsc.h"
#include "lbu/xmalloc.h"

namespace lbu {
namespace stream {

using alg = lbu::ring_spsc::algorithm::mirrored_index<uint32_t>;

static uint32_t continuous_slots(uint32_t offset, uint32_t count, uint32_t n)
{
    return lbu::ring_spsc::algorithm::continuous_slots(offset, count, n);
}

static bool consumer_read(int fd)
{
    eventfd_t v;
    switch( event_fd::read(fd, &v) ) {
    case event_fd::ReadNoError:
    case event_fd::ReadWouldBlock:
        return true;
    default:
        return false;
    }
}

static bool producer_write(int fd)
{
    switch( event_fd::write(fd, event_fd::MaximumValue) ) {
    case event_fd::WriteNoError:
    case event_fd::WriteWouldBlock:
        return true;
    default:
        return false;
    }
}


ring_spsc::input_stream::input_stream()
    : abstract_input_stream(InternalBuffer::Yes)
{
}

ring_spsc::input_stream::~input_stream()
{
}

void ring_spsc::input_stream::reset(array_ref<void> buffer,
                                    int event_fd,
                                    ring_spsc_shared_data* s)
{
    bufferBase = static_cast<char*>(buffer.data());
    const auto n = std::min(buffer.byte_size(), alg::max_size());
    d.ringSize = n;
    d.shared = s;
    d.fd = event_fd;

    d.lastIndex = s->consumer_index.load(std::memory_order_relaxed);
    bufferOffset = alg::offset(d.lastIndex, n);

    update_buffer_size(s, d.lastIndex, d.segmentLimit, n);
}

ssize_t ring_spsc::input_stream::read_stream(array_ref<io::io_vector> buf_array, size_t required_read)
{
    const Mode mode = (required_read > 0) ? Mode::Blocking : Mode::NonBlocking;
    auto dst = io::io_vec_to_array_ref(buf_array[0]).array_static_cast<char>();

    size_t count = 0;
    while( true ) {
        auto buf = next_buffer(mode);
        if( buf.size() == 0 )
            return has_error() ? -1 : ssize_t(count);

        if( buf.size() >= dst.size() ) {
            const auto c = dst.size();
            std::memcpy(dst.data(), buf.data(), c);
            advance_buffer(c);
            return ssize_t(count + c);
        }

        const auto c = buf.size();
        std::memcpy(dst.data(), buf.data(), c);
        advance_whole_buffer();
        count += c;
        dst = dst.sub(c);
    }
}

array_ref<const void> ring_spsc::input_stream::get_read_buffer(Mode mode)
{
    return next_buffer(mode);
}

array_ref<const void> ring_spsc::input_stream::next_buffer(Mode mode)
{
    assert(bufferSize == 0);
    if( statusFlags ) {
        if( mode == Mode::Blocking )
            statusFlags |= StatusError;
        return {};
    }

    auto s = d.shared;
    const auto n = d.ringSize;
    const auto segmentLimit = d.segmentLimit;
    const int fd = d.fd;
    const auto count = bufferOffset - alg::offset(d.lastIndex, n);
    const auto consumerIdx = alg::new_index(d.lastIndex, count, n);

    s->consumer_index.store(consumerIdx, std::memory_order_release);
    d.lastIndex = consumerIdx;
    bufferOffset = alg::offset(consumerIdx, n);

    bool wakeProducer = count > 0;
    if( wakeProducer && s->producer_wake.compare_exchange_strong(wakeProducer, false) ) {
        if( ! consumer_read(fd) )
            goto error;
    }

    if( update_buffer_size(s, consumerIdx, segmentLimit, n) )
        return current_buffer();

    if( ! wakeProducer ) {
        if( ! consumer_read(fd) )
            goto error;
        if( update_buffer_size(s, consumerIdx, segmentLimit, n) )
            return current_buffer();
    }

    s->consumer_wake.store(true);

    if( update_buffer_size(s, consumerIdx, segmentLimit, n) || mode == Mode::NonBlocking )
        return current_buffer();

    if( ! poll::wait_for_event(fd, poll::FlagsReadReady) )
        goto error;

    if( update_buffer_size(s, consumerIdx, segmentLimit, n) )
        return current_buffer();

    if( ! consumer_read(fd) )
        goto error;

    if( update_buffer_size(s, consumerIdx, segmentLimit, n) )
        return current_buffer();

    if( ! poll::wait_for_event(fd, poll::FlagsReadReady) )
        goto error;

    if( ! update_buffer_size(s, consumerIdx, segmentLimit, n) )
        assert(false);
    return current_buffer();

error:
    statusFlags = StatusError;
    return {};
}

bool ring_spsc::input_stream::update_buffer_size(ring_spsc_shared_data* shared,
                                                 uint32_t consumer_index,
                                                 uint32_t segmentLimit,
                                                 uint32_t ringSize)
{
    const auto producer_index = shared->producer_index.load(std::memory_order_acquire);
    const auto n = ringSize;
    auto b = continuous_slots(alg::offset(consumer_index, n),
                              alg::consumer_free_slots(producer_index, consumer_index, n),
                              n);
    bufferSize = std::min(segmentLimit, b);
    statusFlags = bufferSize > 0 ? StatusBufferReady : 0;
    if( bufferSize == 0 && shared->eos.load(std::memory_order_acquire) ) {
        statusFlags = StatusEndOfStream;
        return true;
    }
    return bufferSize > 0;
}

ring_spsc::output_stream::output_stream()
    : abstract_output_stream(InternalBuffer::Yes)
{
}

ring_spsc::output_stream::~output_stream()
{
}

bool ring_spsc::output_stream::set_end_of_stream()
{
    if( statusFlags )
        return false;

    auto s = d.shared;
    const auto n = d.ringSize;
    const int fd = d.fd;
    const auto count = bufferOffset - alg::offset(d.lastIndex, n);
    const auto producerIdx = alg::new_index(d.lastIndex, count, n);

    s->producer_index.store(producerIdx, std::memory_order_release);

    bufferSize = 0;

    d.shared->eos.store(true, std::memory_order_release);
    if( ! producer_write(fd) ) {
        statusFlags = StatusError;
        return false;
    }
    statusFlags = StatusEndOfStream;
    return true;
}

void ring_spsc::output_stream::reset(array_ref<void> buffer,
                                     int event_fd,
                                     ring_spsc_shared_data* s)
{
    bufferBase = static_cast<char*>(buffer.data());
    const auto n = std::min(buffer.byte_size(), alg::max_size());
    d.ringSize = n;
    d.shared = s;
    d.fd = event_fd;

    d.lastIndex = s->producer_index.load(std::memory_order_relaxed);
    bufferOffset = alg::offset(d.lastIndex, n);

    update_buffer_size(s, d.lastIndex, d.segmentLimit, n);
}

ssize_t ring_spsc::output_stream::write_stream(array_ref<io::io_vector> buf_array, Mode mode)
{
    if( statusFlags ) {
        statusFlags |= StatusError;
        return -1;
    }

    auto src = io::io_vec_to_array_ref(buf_array[0]).array_static_cast<char>();

    array_ref<void> buf = current_buffer();
    size_t count = buf.size();

    if( src.size() > 0 ) {
        assert(count < src.size());
        if( count > 0 ) {
            std::memcpy(buf.data(), src.data(), count);
            advance_whole_buffer();
            src = src.sub(count);
        }
    } else {
        assert(count == 0);
        mode = Mode::NonBlocking;
    }

    while( true ) {
        buf = next_buffer(mode);
        if( buf.size() == 0 )
            return has_error() ? -1 : ssize_t(count);

        if( buf.size() >= src.size() ) {
            const auto c = src.size();
            std::memcpy(buf.data(), src.data(), c);
            advance_buffer(c);
            return ssize_t(count + c);
        }

        const auto c = buf.size();
        std::memcpy(buf.data(), src.data(), c);
        advance_whole_buffer();
        count += c;
        src = src.sub(c);
    }
}

array_ref<void> ring_spsc::output_stream::get_write_buffer(Mode mode)
{
    return next_buffer(mode);
}

bool ring_spsc::output_stream::write_buffer_flush(Mode)
{
    next_buffer(Mode::NonBlocking);
    return !has_error();
}

array_ref<void> ring_spsc::output_stream::next_buffer(Mode mode)
{
    if( statusFlags ) {
        assert(bufferSize == 0);
        statusFlags |= StatusError;
        return {};
    }

    auto s = d.shared;
    const auto n = d.ringSize;
    const auto segmentLimit = d.segmentLimit;
    const int fd = d.fd;
    const auto count = bufferOffset - alg::offset(d.lastIndex, n);
    const auto producerIdx = alg::new_index(d.lastIndex, count, n);

    s->producer_index.store(producerIdx, std::memory_order_release);
    d.lastIndex = producerIdx;
    bufferOffset = alg::offset(producerIdx, n);

    bool wakeConsumer = count > 0;
    if( wakeConsumer && s->consumer_wake.compare_exchange_strong(wakeConsumer, false) ) {
        if( ! producer_write(fd) )
            goto error;
    }

    if( update_buffer_size(s, producerIdx, segmentLimit, n) )
        return current_buffer();

    if( wakeConsumer == false ) {
        if( ! producer_write(fd) )
            goto error;
        if( update_buffer_size(s, producerIdx, segmentLimit, n) )
            return current_buffer();
    }

    s->producer_wake.store(true);

    if( update_buffer_size(s, producerIdx, segmentLimit, n) || mode == Mode::NonBlocking )
        return current_buffer();

    if( ! poll::wait_for_event(fd, poll::FlagsWriteReady) )
        goto error;

    if( update_buffer_size(s, producerIdx, segmentLimit, n) )
        return current_buffer();

    if( ! producer_write(fd) )
        goto error;

    if( update_buffer_size(s, producerIdx, segmentLimit, n) )
        return current_buffer();

    if( ! poll::wait_for_event(fd, poll::FlagsWriteReady) )
        goto error;

    if( ! update_buffer_size(s, producerIdx, segmentLimit, n) )
        assert(false);
    return current_buffer();

error:
    statusFlags = StatusError;
    bufferSize = 0;
    return {};
}

bool ring_spsc::output_stream::update_buffer_size(ring_spsc_shared_data* shared,
                                                  uint32_t producer_index,
                                                  uint32_t segmentLimit,
                                                  uint32_t ringSize)
{
    uint32_t consumer_index = shared->consumer_index.load(std::memory_order_acquire);
    const auto n = ringSize;
    auto b = continuous_slots(alg::offset(producer_index, n),
                              alg::producer_free_slots(producer_index, consumer_index, n),
                              n);
    bufferSize = std::min(segmentLimit, b);
    statusFlags = bufferSize > 0 ? StatusBufferReady : 0;
    return bufferSize > 0;
}

bool ring_spsc_shared_data::open_event_fd(int* event_fd)
{
    return event_fd::open(event_fd, 0, lbu::event_fd::FlagsNonBlock) == event_fd::OpenNoError;
}


struct ring_spsc_basic_controller::internal {
    ring_spsc_shared_data shared;
    uint32_t bufsize;
    int fd = -1;
    char* buf;
};

ring_spsc_basic_controller::ring_spsc_basic_controller(uint32_t bufsize)
{
    const auto align = size_t(std::max<long>(sysconf(_SC_LEVEL1_DCACHE_LINESIZE), alignof(internal)));
    const auto padded = align_up(sizeof(internal), align);
    const auto s = std::min(std::max(bufsize, 1u), (uint32_t(1) << 31) - 1);
    char* p = xmalloc_aligned<char>(align, padded + s);
    d = reinterpret_cast<internal*>(p);

    new (d) internal;
    d->bufsize = s;
    d->buf = p + padded;

    ring_spsc_shared_data::open_event_fd(&(d->fd));
}

ring_spsc_basic_controller::~ring_spsc_basic_controller()
{
    if( d->fd >= 0 )
        ::close(d->fd);

    d->~internal();

    ::free(d);
}

bool ring_spsc_basic_controller::pair_streams(ring_spsc::output_stream *out,
                                              ring_spsc::input_stream *in,
                                              uint32_t segment_limit)
{
    if( d->fd < 0 )
        return false;

    auto buf = array_ref<char>(d->buf, d->bufsize);
    ring_spsc::pair_streams(out, in, buf, d->fd, &(d->shared), segment_limit);
    return true;
}


} // namespace stream
} // namespace lbu
