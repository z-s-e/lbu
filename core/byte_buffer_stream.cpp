/* Copyright 2015-2017 Zeno Sebastian Endemann <zeno.endemann@mailbox.org>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "lbu/byte_buffer_stream.h"

namespace lbu {
namespace stream {

byte_buffer_input_stream::byte_buffer_input_stream(array_ref<void> buf)
    : abstract_input_stream(InternalBuffer::Yes)
{
    reset(buf);
}

byte_buffer_input_stream::~byte_buffer_input_stream()
{
}

void byte_buffer_input_stream::reset(array_ref<void> buf)
{
    buffer_base_ptr = static_cast<char*>(buf.data());
    buffer_offset = 0;
    if( buf.byte_size() <= std::numeric_limits<uint32_t>::max() ) {
        buffer_available = buf.byte_size();
        status_flags = 0;
    } else {
        buffer_available = 0;
        status_flags = StatusError;
    }
}

ssize_t byte_buffer_input_stream::read_stream(array_ref<io::io_vector> buf_array, size_t)
{
    if( has_error() )
        return -1;

    assert(buffer_available == 0 || buf_array[0].iov_len > buffer_available);
    const auto r = ssize_t(buffer_available);

    if( buffer_available > 0 ) {
        std::memcpy(buf_array[0].iov_base, buffer_base_ptr + buffer_offset, buffer_available);
        buffer_available = 0;
    }

    status_flags = StatusEndOfStream;
    return r;
}

array_ref<const void> byte_buffer_input_stream::get_read_buffer(Mode)
{
    status_flags = StatusEndOfStream;
    return {};
}


byte_buffer_output_stream::byte_buffer_output_stream()
    : abstract_output_stream(InternalBuffer::Yes)
{
}

byte_buffer_output_stream::~byte_buffer_output_stream()
{
}

void byte_buffer_output_stream::reset(byte_buffer &&buf)
{
    buffer = std::move(buf);
    sync_state();
    status_flags = 0;
}

ssize_t byte_buffer_output_stream::write_stream(array_ref<io::io_vector> buf_array, Mode)
{
    if( status_flags )
        return -1;
    buffer.append_commit(buffer_offset - buffer.size());
    const auto buf = io::io_vec_to_array_ref(buf_array[0]);
    if( buffer.max_size() - buffer.size() > buf.byte_size() ) {
        status_flags = StatusError;
        return -1;
    }
    buffer.append(buf);
    sync_state();
    return ssize_t(buf.byte_size());
}

array_ref<void> byte_buffer_output_stream::get_write_buffer(Mode)
{
    if( status_flags )
        return {};
    buffer.auto_grow_reserve();
    sync_state();
    if( buffer_available == 0 )
        status_flags = StatusError;
    return current_buffer();
}

bool byte_buffer_output_stream::write_buffer_flush(Mode)
{
    if( status_flags )
        return false;
    buffer.append_commit(buffer_offset - buffer.size());
    return true;
}

void byte_buffer_output_stream::sync_state()
{
    buffer_base_ptr = static_cast<char*>(buffer.data());
    buffer_offset = buffer.size();
    buffer_available = buffer.capacity() - buffer.size();
}


} // namespace stream
} // namespace lbu
