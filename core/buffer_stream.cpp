/* Copyright 2015-2016 Zeno Sebastian Endemann <zeno.endemann@googlemail.com>
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
    if( buf.byte_size() < std::numeric_limits<uint32_t>::max() ) {
        bufferSize = buf.byte_size();
        statusFlags = bufferSize > 0 ? StatusBufferReady : 0;
    } else {
        bufferSize = 0;
        statusFlags = StatusError;
    }
}

ssize_t byte_buffer_input_stream::read_stream(array_ref<io::io_vector>, size_t required_read)
{
    if( required_read > 0 ) {
        statusFlags = (StatusError | StatusEndOfStream);
        return -1;
    } else {
        statusFlags = StatusEndOfStream;
        return 0;
    }
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
}

ssize_t byte_buffer_output_stream::write_stream(array_ref<io::io_vector> buf_array, Mode)
{
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
    buffer.auto_grow_reserve();
    sync_state();
    return current_buffer();
}

bool byte_buffer_output_stream::write_buffer_flush(Mode)
{
    buffer.append_commit(bufferOffset - buffer.size());
    return true;
}

void byte_buffer_output_stream::sync_state()
{
    bufferBase = static_cast<char*>(buffer.data());
    bufferOffset = buffer.size();
    bufferSize = buffer.capacity();
    statusFlags = (bufferSize > bufferOffset) ? StatusBufferReady : 0;
}


} // namespace stream
} // namespace lbu
