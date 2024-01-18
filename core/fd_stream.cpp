/* Copyright 2015-2023 Zeno Sebastian Endemann <zeno.endemann@mailbox.org>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "lbu/fd_stream.h"

#include "lbu/dynamic_memory.h"

#include <algorithm>

namespace lbu {
namespace stream {


static bool update_blocking(Mode mode, FdBlockingState block, fd f, int* err)
{
    if( block == FdBlockingState::Automatic )
        return f.set_nonblock(mode == Mode::NonBlocking, err);
    if( (mode == Mode::Blocking) != (block == FdBlockingState::AlwaysBlocking) ) {
        *err = io::ReadBadRequest;
        return false;
    }
    return true;
}

static int set_flag_return_error(uint8_t* status_flags, uint8_t flag)
{
    *status_flags |= flag;
    return -1;
}


fd_input_stream::fd_input_stream(array_ref<void> buffer, fd f, FdBlockingState b)
    : abstract_input_stream(buffer ? InternalBuffer::Yes : InternalBuffer::No)
{
    buffer_base_ptr = static_cast<char*>(buffer.data());
    buffer_capacity = std::min(buffer.byte_size(), size_t(std::numeric_limits<uint32_t>::max()));
    set_descriptor(f, b);
}

fd_input_stream::~fd_input_stream()
{
}

void fd_input_stream::set_descriptor(fd f, FdBlockingState b)
{
    filedes = f;
    fd_blocking = b;
    err = io::ReadNoError;
    buffer_available = 0;
    buffer_offset = 0;
    status_flags = 0;
}

ssize_t fd_input_stream::read_stream(array_ref<io::io_vector> buf_array, size_t required_read)
{
    if( has_error() )
        return -1;
    if( at_end() ) {
        if( required_read == 0 || manages_buffer() )
            return 0;
        err = io::ReadBadRequest;
        return set_flag_return_error(&status_flags, StatusError);
    }

    const Mode mode = (required_read > 0) ? Mode::Blocking : Mode::NonBlocking;
    if( ! update_blocking(mode, fd_blocking, filedes, &err) )
        return set_flag_return_error(&status_flags, StatusError);

    io::io_vector internal_array[2];
    uint32_t buffer_read = 0;

    if( manages_buffer() ) {
        assert(buf_array.size() == 1);
        buffer_read = buffer_available;
        assert(buffer_read == 0 || buf_array[0].iov_len > buffer_read);
        if( buffer_read > 0 ) {
            std::memcpy(buf_array[0].iov_base, buffer_base_ptr + buffer_offset, buffer_read);
            buf_array = io::io_vector_array_advance(buf_array, buffer_read);
            buffer_available = 0;
            if( required_read > 0 ) {
                assert(required_read > buffer_read);
                required_read -= buffer_read;
            }
        }

        if( buf_array[0].iov_len <= buffer_capacity) {
            // Only fill the internal buffer if the requested read size fits in the buffer.
            // Rationale: It is likely that the user will continue reading in large blocks,
            //            where using the internal buffer might be actually worse for
            //            performance (though it might not matter much...)
            internal_array[0] = buf_array[0];
            internal_array[1] = io::io_vec(buffer_base_ptr, buffer_capacity);
            buf_array = array_ref<io::io_vector>(internal_array);
        }
    } else if( buf_array.size() == 0 ) {
        assert(required_read == 0);
        return 0;
    }

    const size_t first_read_request = buf_array[0].iov_len;

    size_t count = 0;
    while( true ) {
        const auto r = io::readv(filedes, buf_array);
        if( r.size > 0 ) {
            count += size_t(r.size);

            if( count < required_read ) {
                buf_array = io::io_vector_array_advance(buf_array, size_t(r.size));
                continue;
            }

            if( manages_buffer() && count > first_read_request ) {
                buffer_offset = 0;
                buffer_available = uint32_t(count - first_read_request);
                return ssize_t(first_read_request + buffer_read);
            }

            return ssize_t(count + buffer_read);
        } else if( r.size == 0 ) {
            if( mode == Mode::Blocking ) {
                status_flags = StatusEndOfStream;
                if( manages_buffer() ) {
                    return ssize_t(count + buffer_read);
                } else {
                    err = io::ReadBadRequest;
                    return set_flag_return_error(&status_flags, StatusError);
                }
            } else {
                if( ! io::io_vector_array_has_zero_size(buf_array) )
                    status_flags = StatusEndOfStream;
                return ssize_t(buffer_read);
            }
        } else if( r.status == io::ReadWouldBlock && mode == Mode::NonBlocking ) {
            return ssize_t(buffer_read);
        } else {
            err = r.status;
            return set_flag_return_error(&status_flags, StatusError);
        }
    }
}

array_ref<const void> fd_input_stream::get_read_buffer(Mode mode)
{
    if( status_flags )
        return {};
    if( ! update_blocking(mode, fd_blocking, filedes, &err) ) {
        status_flags = StatusError;
        return {};
    }

    const auto r = io::read(filedes, array_ref<char>(buffer_base_ptr, buffer_capacity));
    if( r.size > 0 ) {
        buffer_offset = 0;
        buffer_available = uint32_t(r.size);
        return current_buffer();
    } else if( r.size == 0 ) {
        status_flags = StatusEndOfStream;
    } else if( mode == Mode::NonBlocking && r.status == io::ReadWouldBlock ) {
    } else {
        err = r.status;
        status_flags = StatusError;
    }

    return {};
}

fd_output_stream::fd_output_stream(array_ref<void> buffer, fd f, FdBlockingState b)
    : abstract_output_stream(buffer ? InternalBuffer::Yes : InternalBuffer::No)
{
    buffer_base_ptr = static_cast<char*>(buffer.data());
    buffer_capacity = std::min(buffer.byte_size(), size_t(std::numeric_limits<uint32_t>::max()));
    set_descriptor(f, b);
}

fd_output_stream::~fd_output_stream()
{
}

void fd_output_stream::set_descriptor(fd f, FdBlockingState b)
{
    filedes = f;
    fd_blocking = b;
    err = io::WriteNoError;
    reset_buffer();
    if( ! f )
        buffer_available = 0;
    status_flags = 0;
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
    if( status_flags )
        return -1;
    if( ! update_blocking(mode, fd_blocking, filedes, &err) )
        return set_flag_return_error(&status_flags, StatusError);

    io::io_vector internal_array[2];
    ssize_t internal_write_size = 0;

    if( manages_buffer() ) {
        assert(buf_array.size() == 1);

        internal_write_size = buffer_offset - buffer_write_offset;
        if( internal_write_size > 0 ) {
            internal_array[0] = io::io_vec(buffer_base_ptr + buffer_write_offset, size_t(internal_write_size));
            internal_array[1] = buf_array[0];
            buf_array = array_ref<io::io_vector>(internal_array);
        }
    }

    if( mode == Mode::Blocking ) {
        const ssize_t sum = ssize_t(io::io_vector_array_size_sum(buf_array));
        ssize_t count = 0;

        while( count < sum ) {
            const auto r = io::writev(filedes, buf_array);
            if( r.size < 0 ) {
                buffer_available = 0;
                err = r.status;
                return set_flag_return_error(&status_flags, StatusError);
            }
            count += r.size;
            buf_array = io::io_vector_array_advance(buf_array, size_t(r.size));
        }

        if( manages_buffer() )
            reset_buffer();

        return sum - internal_write_size;
    } else {
        const auto r = io::writev(filedes, buf_array);
        if( r.size >= 0 ) {
            if( r.size >= internal_write_size ) {
                if( manages_buffer() )
                    reset_buffer();
                return r.size - internal_write_size;
            } else {
                buffer_write_offset += uint32_t(r.size);
                return 0;
            }
        } else if( r.status == io::WriteWouldBlock ) {
            return 0;
        } else {
            buffer_available = 0;
            err = r.status;
            return set_flag_return_error(&status_flags, StatusError);
        }
    }
}

bool fd_output_stream::buffer_flush(Mode mode)
{
    auto b = io::io_vec(nullptr, 0);
    if( write_fd(array_ref_one_element(&b), mode) < 0 )
        return false;
    return (buffer_offset == 0);
}

void fd_output_stream::reset_buffer()
{
    buffer_offset = 0;
    buffer_write_offset = 0;
    buffer_available = buffer_capacity;
}


socket_stream_pair::socket_stream_pair(uint32_t bufsize)
    : in(array_ref<char>(xmalloc_bytes<char>(bufsize), bufsize))
    , out(array_ref<char>(xmalloc_bytes<char>(bufsize), bufsize))
{
}

socket_stream_pair::socket_stream_pair(uint32_t bufsize_read, uint32_t bufsize_write)
    : in(array_ref<char>(xmalloc_bytes<char>(bufsize_read), bufsize_read))
    , out(array_ref<char>(xmalloc_bytes<char>(bufsize_write), bufsize_write))
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
    : out(array_ref<char>(xmalloc_bytes<char>(bufsize), bufsize))
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
    : in(array_ref<char>(xmalloc_bytes<char>(bufsize), bufsize))
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
