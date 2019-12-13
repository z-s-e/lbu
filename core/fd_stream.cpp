/* Copyright 2015-2019 Zeno Sebastian Endemann <zeno.endemann@googlemail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "lbu/fd_stream.h"

#include "lbu/pipe.h"
#include "lbu/xmalloc.h"

#include <algorithm>

namespace lbu {
namespace stream {


static inline void update_blocking(Mode mode, FdBlockingState *block, fd f)
{
    // Ignore possible errors from setting nonblock, because if there is an
    // error, the following read/write operations should fail as well.
    if( mode == Mode::Blocking ) {
        if( *block != FdBlockingState::Blocking ) {
            if( f.set_nonblock(false) )
                *block = FdBlockingState::Blocking;
        }
    } else {
        if( *block != FdBlockingState::NonBlocking ) {
            if( f.set_nonblock(true) )
                *block = FdBlockingState::NonBlocking;
        }
    }
}


fd_input_stream::fd_input_stream(array_ref<void> buffer,
                                 fd f,
                                 FdBlockingState b)
    : abstract_input_stream(buffer ? InternalBuffer::Yes : InternalBuffer::No)
{
    bufferBase = static_cast<char*>(buffer.data());
    bufferCapacity = std::min(buffer.byte_size(), size_t(std::numeric_limits<uint32_t>::max()));
    set_descriptor(f, b);
}

fd_input_stream::~fd_input_stream()
{
}

void fd_input_stream::set_descriptor(fd f, FdBlockingState b)
{
    filedes = f;
    fdBlocking = b;
    err = io::ReadNoError;
    bufferAvailable = 0;
    bufferOffset = 0;
    statusFlags = 0;
}

ssize_t fd_input_stream::read_stream(array_ref<io::io_vector> buf_array, size_t required_read)
{
    if( has_error() )
        return -1;

    const Mode mode = (required_read > 0) ? Mode::Blocking : Mode::NonBlocking;
    update_blocking(mode, &fdBlocking, filedes);

    io::io_vector internalArray[2];
    uint32_t bufferRead = 0;

    if( manages_buffer() ) {
        assert(buf_array.size() == 1);
        bufferRead = bufferAvailable;
        assert(buf_array[0].iov_len > bufferRead);
        if( bufferRead > 0 ) {
            std::memcpy(buf_array[0].iov_base, bufferBase + bufferOffset, bufferRead);
            buf_array = io::io_vector_array_advance(buf_array, bufferRead);
            bufferAvailable = 0;
            if( required_read > 0 ) {
                assert(required_read > bufferRead);
                required_read -= bufferRead;
            }
        }

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
        const auto r = io::readv(filedes, buf_array);
        if( r.size > 0 ) {
            count += size_t(r.size);

            if( count < required_read ) {
                buf_array = io::io_vector_array_advance(buf_array, size_t(r.size));
                continue;
            }

            if( manages_buffer() && count > firstReadRequest ) {
                bufferOffset = 0;
                bufferAvailable = uint32_t(count - firstReadRequest);
                return ssize_t(firstReadRequest + bufferRead);
            }

            return ssize_t(count + bufferRead);
        } else if( r.size == 0 ) {
            if( mode == Mode::Blocking ) {
                if( io::io_vector_array_has_zero_size(buf_array) ) {
                    err = io::ReadBadRequest;
                    statusFlags = StatusError;
                    return -1;
                } else {
                    statusFlags = StatusEndOfStream;
                    return ssize_t(count + bufferRead);
                }
            } else {
                if( ! io::io_vector_array_has_zero_size(buf_array) )
                    statusFlags = StatusEndOfStream;
                return bufferRead;
            }
        } else if( r.status == io::ReadWouldBlock && mode == Mode::NonBlocking ) {
            return bufferRead;
        } else {
            err = r.status;
            statusFlags = StatusError;
            return -1;
        }
    }
}

array_ref<const void> fd_input_stream::get_read_buffer(Mode mode)
{
    if( has_error() )
        return {};
    update_blocking(mode, &fdBlocking, filedes);

    const auto r = io::read(filedes, array_ref<char>(bufferBase, bufferCapacity));
    if( r.size > 0 ) {
        bufferOffset = 0;
        bufferAvailable = uint32_t(r.size);
        return current_buffer();
    } else if( r.size == 0 ) {
        statusFlags = StatusEndOfStream;
    } else if( mode == Mode::NonBlocking && r.status == io::ReadWouldBlock ) {
    } else {
        err = r.status;
        statusFlags = StatusError;
    }

    return {};
}

fd_output_stream::fd_output_stream(array_ref<void> buffer,
                                   fd f,
                                   FdBlockingState b)
    : abstract_output_stream(buffer ? InternalBuffer::Yes : InternalBuffer::No)
{
    bufferBase = static_cast<char*>(buffer.data());
    bufferCapacity = std::min(buffer.byte_size(), size_t(std::numeric_limits<uint32_t>::max()));
    set_descriptor(f, b);
}

fd_output_stream::~fd_output_stream()
{
}

void fd_output_stream::set_descriptor(fd f, FdBlockingState b)
{
    filedes = f;
    fdBlocking = b;
    err = io::WriteNoError;
    reset_buffer();
    if( ! f )
        bufferAvailable = 0;
}

ssize_t fd_output_stream::write_stream(array_ref<io::io_vector> buf_array, Mode mode)
{
    return write_fd(buf_array, mode);
}

array_ref<void> fd_output_stream::get_write_buffer(Mode mode)
{
    buffer_flush(mode);
    return current_buffer();
}

bool fd_output_stream::write_buffer_flush(Mode mode)
{
    return buffer_flush(mode);
}

ssize_t fd_output_stream::write_fd(array_ref<io::io_vector> buf_array, Mode mode)
{
    if( has_error() )
        return -1;
    update_blocking(mode, &fdBlocking, filedes);

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
            const auto r = io::writev(filedes, buf_array);
            if( r.size < 0 ) {
                statusFlags = StatusError;
                err = r.status;
                bufferAvailable = 0;
                return -1;
            }
            count += r.size;
            buf_array = io::io_vector_array_advance(buf_array, size_t(r.size));
        }

        if( manages_buffer() )
            reset_buffer();

        return sum - internalWriteSize;
    } else {
        const auto r = io::writev(filedes, buf_array);
        if( r.size >= 0 ) {
            if( r.size >= internalWriteSize ) {
                if( manages_buffer() )
                    reset_buffer();
                return r.size - internalWriteSize;
            } else {
                bufferWriteOffset += r.size;
                return 0;
            }
        } else if( r.status == io::WriteWouldBlock ) {
            return 0;
        } else {
            statusFlags = StatusError;
            err = r.status;
            bufferAvailable = 0;
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
    bufferAvailable = bufferCapacity;
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
    if( in.descriptor() )
        in.descriptor().close();
    ::free(in.buffer_base());
    ::free(out.buffer_base());
}

unique_fd socket_stream_pair::take_reset(unique_fd f, FdBlockingState b)
{
    fd oldfd = in.descriptor();
    fd newfd = f.release();
    in.set_descriptor(newfd, b);
    out.set_descriptor(newfd, b);
    return unique_fd(oldfd);
}


managed_fd_output_stream::managed_fd_output_stream(uint32_t bufsize)
    : out(array_ref<char>(xmalloc<char>(bufsize), bufsize))
{
}

managed_fd_output_stream::~managed_fd_output_stream()
{
    if( out.descriptor() )
        out.descriptor().close();
    ::free(out.buffer_base());
}

void managed_fd_output_stream::reset(unique_fd f, FdBlockingState b)
{
    if( out.descriptor() )
        out.descriptor().close();
    out.set_descriptor(f.release(), b);
}


managed_fd_input_stream::managed_fd_input_stream(uint32_t bufsize)
    : in(array_ref<char>(xmalloc<char>(bufsize), bufsize))
{
}

managed_fd_input_stream::~managed_fd_input_stream()
{
    if( in.descriptor() )
        in.descriptor().close();
    ::free(in.buffer_base());
}

void managed_fd_input_stream::reset(unique_fd f, FdBlockingState b)
{
    if( in.descriptor() )
        in.descriptor().close();
    in.set_descriptor(f.release(), b);
}

} // namespace stream
} // namespace lbu
