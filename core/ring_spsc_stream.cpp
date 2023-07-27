/* Copyright 2015-2020 Zeno Sebastian Endemann <zeno.endemann@mailbox.org>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "lbu/ring_spsc_stream.h"

#include "lbu/dynamic_memory.h"
#include "lbu/eventfd.h"
#include "lbu/poll.h"
#include "lbu/ring_spsc.h"

namespace lbu {
namespace stream {

using alg = lbu::ring_spsc::algorithm::mirrored_index<uint32_t>;

static uint32_t continuous_slots(uint32_t offset, uint32_t count, uint32_t n)
{
    return lbu::ring_spsc::algorithm::continuous_slots(offset, count, n);
}

static bool consumer_read(fd f)
{
    eventfd_t v;
    switch( event_fd::read(f, &v) ) {
    case event_fd::ReadNoError:
    case event_fd::ReadWouldBlock:
        return true;
    default:
        return false;
    }
}

static bool producer_write(fd f)
{
    switch( event_fd::write(f, event_fd::MaximumValue) ) {
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
                                    fd event_fd,
                                    ring_spsc_shared_data* s)
{
    buffer_base_ptr = static_cast<char*>(buffer.data());
    const auto n = uint32_t(std::min(buffer.byte_size(), alg::max_size()));
    d.ring_size = n;
    d.shared = s;
    d.filedes = event_fd;

    d.last_index = s->consumer_index.load(std::memory_order_relaxed);
    buffer_offset = alg::offset(d.last_index, n);
    update_buffer_size(s, d.last_index, d.segment_limit, n);

    status_flags = 0;
}

ssize_t ring_spsc::input_stream::read_stream(array_ref<io::io_vector> buf_array, size_t required_read)
{
    const Mode mode = (required_read > 0) ? Mode::Blocking : Mode::NonBlocking;
    auto dst = io::io_vec_to_array_ref(buf_array[0]).array_static_cast<char>();

    size_t count = buffer_available;
    if( count > 0 ) {
        std::memcpy(dst.data(), buffer_base_ptr + buffer_offset, count);
        advance_buffer(count);
        dst = dst.sub(count);
    }

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
    assert(buffer_available == 0);
    if( status_flags )
        return {};

    ring_spsc_shared_data* const s = d.shared;
    assert(s != nullptr);
    const auto n = d.ring_size;
    const auto segment_limit = d.segment_limit;
    const fd f = d.filedes;
    const auto count = buffer_offset - alg::offset(d.last_index, n);
    const auto consumer_idx = alg::new_index(d.last_index, count, n);

    s->consumer_index.store(consumer_idx, std::memory_order_release);
    d.last_index = consumer_idx;
    buffer_offset = alg::offset(consumer_idx, n);

    bool wake_producer = count > 0;
    if( wake_producer && s->producer_wake.compare_exchange_strong(wake_producer, false) ) {
        if( ! consumer_read(f) )
            goto error;
    }

    if( update_buffer_size(s, consumer_idx, segment_limit, n) )
        return current_buffer();

    if( ! wake_producer ) {
        if( ! consumer_read(f) )
            goto error;
        if( update_buffer_size(s, consumer_idx, segment_limit, n) )
            return current_buffer();
    }

    s->consumer_wake.store(true);

    if( update_buffer_size(s, consumer_idx, segment_limit, n) || mode == Mode::NonBlocking )
        return current_buffer();

    if( ! poll::wait_for_event(f, poll::FlagsReadReady) )
        goto error;

    if( update_buffer_size(s, consumer_idx, segment_limit, n) )
        return current_buffer();

    if( ! consumer_read(f) )
        goto error;

    if( update_buffer_size(s, consumer_idx, segment_limit, n) )
        return current_buffer();

    if( ! poll::wait_for_event(f, poll::FlagsReadReady) )
        goto error;

    if( ! update_buffer_size(s, consumer_idx, segment_limit, n) )
        assert(false);
    return current_buffer();

error:
    status_flags = StatusError;
    return {};
}

bool ring_spsc::input_stream::update_buffer_size(ring_spsc_shared_data* shared,
                                                 uint32_t consumer_index,
                                                 uint32_t segment_limit,
                                                 uint32_t ring_size)
{
    const auto producer_index = shared->producer_index.load(std::memory_order_acquire);
    const auto n = ring_size;
    auto b = continuous_slots(alg::offset(consumer_index, n),
                              alg::consumer_free_slots(producer_index, consumer_index, n),
                              n);
    buffer_available = std::min(segment_limit, b);
    if( buffer_available == 0 && shared->eos.load(std::memory_order_acquire) ) {
        status_flags = StatusEndOfStream;
        return true;
    }
    return buffer_available > 0;
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
    if( status_flags )
        return false;

    auto s = d.shared;
    const auto n = d.ring_size;
    const fd f = d.filedes;
    const auto count = buffer_offset - alg::offset(d.last_index, n);
    const auto producer_idx = alg::new_index(d.last_index, count, n);

    s->producer_index.store(producer_idx, std::memory_order_release);

    buffer_available = 0;

    d.shared->eos.store(true, std::memory_order_release);
    if( ! producer_write(f) ) {
        status_flags = StatusError;
        return false;
    }
    status_flags = StatusEndOfStream;
    return true;
}

void ring_spsc::output_stream::reset(array_ref<void> buffer,
                                     fd event_fd,
                                     ring_spsc_shared_data* s)
{
    buffer_base_ptr = static_cast<char*>(buffer.data());
    const auto n = uint32_t(std::min(buffer.byte_size(), alg::max_size()));
    d.ring_size = n;
    d.shared = s;
    d.filedes = event_fd;

    d.last_index = s->producer_index.load(std::memory_order_relaxed);
    buffer_offset = alg::offset(d.last_index, n);
    update_buffer_size(s, d.last_index, d.segment_limit, n);

    status_flags = 0;
}

ssize_t ring_spsc::output_stream::write_stream(array_ref<io::io_vector> buf_array, Mode mode)
{
    if( status_flags ) {
        status_flags |= StatusError;
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
    if( status_flags ) {
        assert(buffer_available == 0);
        status_flags |= StatusError;
        return {};
    }

    auto s = d.shared;
    const auto n = d.ring_size;
    const auto segment_simit = d.segment_limit;
    const fd f = d.filedes;
    const auto count = buffer_offset - alg::offset(d.last_index, n);
    const auto producer_idx = alg::new_index(d.last_index, count, n);

    s->producer_index.store(producer_idx, std::memory_order_release);
    d.last_index = producer_idx;
    buffer_offset = alg::offset(producer_idx, n);

    bool wake_consumer = count > 0;
    if( wake_consumer && s->consumer_wake.compare_exchange_strong(wake_consumer, false) ) {
        if( ! producer_write(f) )
            goto error;
    }

    if( update_buffer_size(s, producer_idx, segment_simit, n) )
        return current_buffer();

    if( ! wake_consumer ) {
        if( ! producer_write(f) )
            goto error;
        if( update_buffer_size(s, producer_idx, segment_simit, n) )
            return current_buffer();
    }

    s->producer_wake.store(true);

    if( update_buffer_size(s, producer_idx, segment_simit, n) || mode == Mode::NonBlocking )
        return current_buffer();

    if( ! poll::wait_for_event(f, poll::FlagsWriteReady) )
        goto error;

    if( update_buffer_size(s, producer_idx, segment_simit, n) )
        return current_buffer();

    if( ! producer_write(f) )
        goto error;

    if( update_buffer_size(s, producer_idx, segment_simit, n) )
        return current_buffer();

    if( ! poll::wait_for_event(f, poll::FlagsWriteReady) )
        goto error;

    if( ! update_buffer_size(s, producer_idx, segment_simit, n) )
        assert(false);
    return current_buffer();

error:
    status_flags = StatusError;
    buffer_available = 0;
    return {};
}

bool ring_spsc::output_stream::update_buffer_size(ring_spsc_shared_data* shared,
                                                  uint32_t producer_index,
                                                  uint32_t segment_limit,
                                                  uint32_t ring_size)
{
    uint32_t consumer_index = shared->consumer_index.load(std::memory_order_acquire);
    const auto n = ring_size;
    auto b = continuous_slots(alg::offset(producer_index, n),
                              alg::producer_free_slots(producer_index, consumer_index, n),
                              n);
    buffer_available = std::min(segment_limit, b);
    return buffer_available > 0;
}

event_fd::open_result ring_spsc_shared_data::open_event_fd()
{
    return event_fd::open(0, lbu::event_fd::FlagsNonBlock);
}


struct ring_spsc_basic_controller::internal {
    ring_spsc_shared_data shared;
    uint32_t bufsize;
    fd filedes = {};
    char* buf;
};

ring_spsc_basic_controller::ring_spsc_basic_controller(uint32_t bufsize)
{
    const auto align = memory_interference_alignment();
    bufsize = std::min<uint32_t>(bufsize, alg::max_size());
    assert(bufsize > 0);
    dynamic_struct s;
    s.add_member<internal>(1, align);
    auto buffer_offset = s.add_member_raw({bufsize, align});
    void* p = xmalloc(s.storage());

    d = new (p) internal;
    d->bufsize = bufsize;
    d->buf = static_cast<char*>(s.resolve(p, buffer_offset));

    auto efd = ring_spsc_shared_data::open_event_fd();
    if( efd.status != event_fd::OpenNoError)
        unexpected_system_error(efd.status);
    d->filedes = efd.f.release();
}

ring_spsc_basic_controller::~ring_spsc_basic_controller()
{
    d->filedes.close();
    d->~internal();
    ::free(d);
}

bool ring_spsc_basic_controller::pair_streams(ring_spsc::output_stream *out,
                                              ring_spsc::input_stream *in,
                                              uint32_t segment_limit)
{
    if( ! d->filedes )
        return false;

    auto buf = array_ref<char>(d->buf, d->bufsize);
    ring_spsc::pair_streams(out, in, buf, d->filedes, &(d->shared), segment_limit);
    return true;
}


} // namespace stream
} // namespace lbu
