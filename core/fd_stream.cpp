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

#include "lbu/fd_stream.h"

#include "lbu/pipe.h"
#include "lbu/xmalloc.h"

#include <algorithm>

namespace lbu {
namespace stream {


static inline void update_blocking(Mode mode, FdBlockingState *block, int fd)
{
    if( mode == Mode::Blocking ) {
        if( *block != FdBlockingState::Blocking ) {
            if( fd::set_nonblock(fd, false) )
                *block = FdBlockingState::Blocking;
        }
    } else {
        if( *block != FdBlockingState::NonBlocking ) {
            if( fd::set_nonblock(fd, true) )
                *block = FdBlockingState::NonBlocking;
        }
    }
}


fd_input_stream::fd_input_stream(array_ref<void> buffer,
                                 int filedes,
                                 FdBlockingState b)
    : abstract_input_stream(buffer ? InternalBuffer::Yes : InternalBuffer::No)
    , fdBlocking(FdBlockingState::Unknown)
    , fd(-1)
    , err(io::ReadNoError)
{
    bufferBase = static_cast<char*>(buffer.data());
    bufferCapacity = std::min(buffer.byte_size(), size_t(std::numeric_limits<uint32_t>::max()));
    if( filedes >= 0 )
        set_descriptor(filedes, b);
}

fd_input_stream::~fd_input_stream()
{
}

void fd_input_stream::set_descriptor(int filedes, FdBlockingState b)
{
    fd = filedes;
    fdBlocking = b;
    err = io::ReadNoError;
    bufferSize = 0;
    bufferOffset = 0;
    statusFlags = 0;
}

ssize_t fd_input_stream::read_stream(array_ref<io::io_vector> buf_array, size_t required_read)
{
    const Mode mode = (required_read > 0) ? Mode::Blocking : Mode::NonBlocking;
    if( has_error() )
        return -1;
    update_blocking(mode, &fdBlocking, fd);

    io::io_vector internalArray[2];

    if( manages_buffer() ) {
        assert(buf_array.size() == 1);
        if( buf_array[0].iov_len <= bufferCapacity) {
            // Only fill the internal buffer if the requested read size fits in the buffer.
            // Rationale: It is likely that the user will continue reading in large blocks,
            //            where using the internal buffer might be actually worse for
            //            performance (though it might not matter much...)
            internalArray[0] = buf_array[0];
            internalArray[1] = io::io_vec(bufferBase, bufferCapacity);
            buf_array = array_ref<io::io_vector>(internalArray);
        }
    } else if( buf_array.size() == 0 ) {
        if( mode == Mode::Blocking ) {
            err = io::ReadBadRequest;
            statusFlags = StatusError;
            return -1;
        } else {
            return 0;
        }
    }

    const size_t firstReadRequest = buf_array[0].iov_len;

    size_t count = 0;
    while( true ) {
        ssize_t r;
        int e = io::readv(fd, buf_array, &r);
        if( r > 0 ) {
            count += size_t(r);

            if( count < required_read ) {
                buf_array = io::io_vector_array_advance(buf_array, size_t(r));
                continue;
            }

            if( manages_buffer() && count > firstReadRequest ) {
                bufferOffset = 0;
                bufferSize = uint32_t(count - firstReadRequest);
                statusFlags = StatusBufferReady;
                return ssize_t(firstReadRequest);
            }

            statusFlags = 0;
            return ssize_t(count);
        } else if( r == 0 ) {
            if( mode == Mode::Blocking ) {
                if( io::io_vector_array_has_zero_size(buf_array) ) {
                    err = io::ReadBadRequest;
                    statusFlags = StatusError;
                } else {
                    err = io::ReadIOError;
                    statusFlags = (StatusError | StatusEndOfStream);
                }
                return -1;
            } else {
                if( ! io::io_vector_array_has_zero_size(buf_array) )
                    statusFlags = StatusEndOfStream;
                return 0;
            }
        } else if( e == io::ReadWouldBlock && mode == Mode::NonBlocking ) {
            return 0;
        } else {
            err = e;
            statusFlags = StatusError;
            return -1;
        }
    }
}

array_ref<const void> fd_input_stream::get_read_buffer(Mode mode)
{
    if( has_error() )
        return {};
    update_blocking(mode, &fdBlocking, fd);

    ssize_t r;
    int e = io::read(fd, array_ref<char>(bufferBase, bufferCapacity), &r);
    if( r > 0 ) {
        bufferOffset = 0;
        bufferSize = uint32_t(r);
        statusFlags = StatusBufferReady;
        return current_buffer();
    } else if( r == 0 ) {
        statusFlags = StatusEndOfStream;
    } else if( mode == Mode::NonBlocking && e == io::ReadWouldBlock ) {
    } else {
        err = e;
        statusFlags = StatusError;
    }

    return {};
}

fd_output_stream::fd_output_stream(array_ref<void> buffer,
                                   int filedes,
                                   FdBlockingState b)
    : abstract_output_stream(buffer ? InternalBuffer::Yes : InternalBuffer::No)
    , fdBlocking(FdBlockingState::Unknown)
    , fd(-1)
    , bufferWriteOffset(0)
    , err(io::WriteNoError)
{
    bufferBase = static_cast<char*>(buffer.data());
    bufferCapacity = std::min(buffer.byte_size(), size_t(std::numeric_limits<uint32_t>::max()));
    if( filedes >= 0 )
        set_descriptor(filedes, b);
}

fd_output_stream::~fd_output_stream()
{
}

void fd_output_stream::set_descriptor(int filedes, FdBlockingState b)
{
    fd = filedes;
    fdBlocking = b;
    err = io::WriteNoError;
    reset_buffer();
}

ssize_t fd_output_stream::write_stream(array_ref<io::io_vector> buf_array, Mode mode)
{
    return write_fd(buf_array, mode);
}

array_ref<void> fd_output_stream::get_write_buffer(Mode mode)
{
    buffer_flush(mode);
    return (statusFlags & StatusBufferReady) ? current_buffer() : array_ref<char>();
}

bool fd_output_stream::write_buffer_flush(Mode mode)
{
    return buffer_flush(mode);
}

ssize_t fd_output_stream::write_fd(array_ref<io::io_vector> buf_array, Mode mode)
{
    if( has_error() )
        return -1;
    update_blocking(mode, &fdBlocking, fd);

    io::io_vector internalArray[2];
    uint32_t internalWriteSize = 0;

    if( manages_buffer() ) {
        assert(buf_array.size() == 1);

        internalWriteSize = bufferOffset - bufferWriteOffset;
        if( internalWriteSize > 0 ) {
            internalArray[0] = io::io_vec(bufferBase + bufferWriteOffset,
                                          internalWriteSize);
            internalArray[1] = buf_array[0];
            buf_array = array_ref<io::io_vector>(internalArray);
        }
    }

    if( mode == Mode::Blocking ) {
        const ssize_t sum = ssize_t(io::io_vector_array_size_sum(buf_array));
        ssize_t count = 0;

        while( count < sum ) {
            ssize_t r;
            int e = io::writev(fd, buf_array, &r);
            if( r < 0 ) {
                statusFlags = StatusError;
                err = e;
                return -1;
            }
            count += r;
            buf_array = io::io_vector_array_advance(buf_array, size_t(r));
        }

        if( manages_buffer() )
            reset_buffer();

        return sum - internalWriteSize;
    } else {
        ssize_t r;
        int e = io::writev(fd, buf_array, &r);
        if( r >= 0 ) {
            if( r >= internalWriteSize ) {
                if( manages_buffer() )
                    reset_buffer();
                return r - internalWriteSize;
            } else {
                bufferWriteOffset += r;
                return 0;
            }
        } else if( e == io::WriteWouldBlock ) {
            return 0;
        } else {
            statusFlags = StatusError;
            err = e;
            return -1;
        }
    }
}

bool fd_output_stream::buffer_flush(Mode mode)
{
    auto b = io::io_vec(nullptr, 0);
    if( write_fd(array_ref_one_element(&b), mode) < 0 )
        return false;
    return (bufferOffset == 0);
}

void fd_output_stream::reset_buffer()
{
    bufferOffset = 0;
    bufferWriteOffset = 0;
    bufferSize = bufferCapacity;
    statusFlags = manages_buffer() ? StatusBufferReady : 0;
}


socket_stream_pair::socket_stream_pair(uint32_t bufsize)
    : in(array_ref<char>(xmalloc<char>(bufsize), bufsize))
    , out(array_ref<char>(xmalloc<char>(bufsize), bufsize))
{
}

socket_stream_pair::socket_stream_pair(uint32_t bufsize_read, uint32_t bufsize_write)
    : in(array_ref<char>(xmalloc<char>(bufsize_read), bufsize_read))
    , out(array_ref<char>(xmalloc<char>(bufsize_write), bufsize_write))
{
}

socket_stream_pair::~socket_stream_pair()
{
    if( in.descriptor() >= 0 )
        ::close(in.descriptor());
    ::free(in.buffer_base());
    ::free(out.buffer_base());
}

fd::unique_fd socket_stream_pair::take_reset(fd::unique_fd filedes, FdBlockingState b)
{
    int old = in.descriptor();
    int fd = filedes.release();
    in.set_descriptor(fd, b);
    out.set_descriptor(fd, b);
    return fd::unique_fd(old);
}


managed_fd_output_stream::managed_fd_output_stream(uint32_t bufsize)
    : out(array_ref<char>(xmalloc<char>(bufsize), bufsize))
{
}

managed_fd_output_stream::~managed_fd_output_stream()
{
    if( out.descriptor() >= 0 )
        ::close(out.descriptor());
    ::free(out.buffer_base());
}

void managed_fd_output_stream::reset(fd::unique_fd filedes, FdBlockingState b)
{
    if( out.descriptor() >= 0 )
        ::close(out.descriptor());
    out.set_descriptor(filedes.release(), b);
}


managed_fd_input_stream::managed_fd_input_stream(uint32_t bufsize)
    : in(array_ref<char>(xmalloc<char>(bufsize), bufsize))
{
}

managed_fd_input_stream::~managed_fd_input_stream()
{
    if( in.descriptor() >= 0 )
        ::close(in.descriptor());
    ::free(in.buffer_base());
}

void managed_fd_input_stream::reset(fd::unique_fd filedes, FdBlockingState b)
{
    if( in.descriptor() >= 0 )
        ::close(in.descriptor());
    in.set_descriptor(filedes.release(), b);
}

} // namespace stream
} // namespace lbu
