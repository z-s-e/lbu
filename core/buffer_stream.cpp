/* Copyright 2015-2017 Zeno Sebastian Endemann <zeno.endemann@googlemail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "lbu/buffer_stream.h"

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
    bufferBase = static_cast<char*>(buf.data());
    bufferOffset = 0;
    if( buf.byte_size() <= std::numeric_limits<uint32_t>::max() ) {
        bufferAvailable = buf.byte_size();
        statusFlags = 0;
    } else {
        bufferAvailable = 0;
        statusFlags = StatusError;
    }
}

ssize_t byte_buffer_input_stream::read_stream(array_ref<io::io_vector> buf_array, size_t)
{
    const ssize_t r = bufferAvailable;
    if( r > 0 ) {
        std::memcpy(buf_array[0].iov_base, bufferBase + bufferOffset, bufferAvailable);
        bufferAvailable = 0;
    }

    statusFlags = StatusEndOfStream;
    return r;
}

array_ref<const void> byte_buffer_input_stream::get_read_buffer(Mode)
{
    statusFlags = StatusEndOfStream;
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
    statusFlags = 0;
}

ssize_t byte_buffer_output_stream::write_stream(array_ref<io::io_vector> buf_array, Mode)
{
    if( statusFlags )
        return -1;
    buffer.append_commit(bufferOffset - buffer.size());
    const auto buf = io::io_vec_to_array_ref(buf_array[0]);
    if( buffer.max_size() - buffer.size() > buf.byte_size() ) {
        statusFlags = StatusError;
        return -1;
    }
    buffer.append(buf);
    sync_state();
    return ssize_t(buf.byte_size());
}

array_ref<void> byte_buffer_output_stream::get_write_buffer(Mode)
{
    if( statusFlags )
        return {};
    buffer.auto_grow_reserve();
    sync_state();
    if( bufferAvailable == 0 )
        statusFlags = StatusError;
    return current_buffer();
}

bool byte_buffer_output_stream::write_buffer_flush(Mode)
{
    if( statusFlags )
        return false;
    buffer.append_commit(bufferOffset - buffer.size());
    return true;
}

void byte_buffer_output_stream::sync_state()
{
    bufferBase = static_cast<char*>(buffer.data());
    bufferOffset = buffer.size();
    bufferAvailable = buffer.capacity() - buffer.size();
}


} // namespace stream
} // namespace lbu
