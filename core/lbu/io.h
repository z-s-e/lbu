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

#ifndef LIBLBU_IO_H
#define LIBLBU_IO_H

#include "lbu/array_ref.h"

#include <cerrno>
#include <unistd.h>
#include <utility>
#include <sys/uio.h>

namespace lbu {
namespace io {

    using io_vector = struct iovec;

    inline io_vector io_vec(void* buf, size_t size)
    {
        struct iovec v;
        v.iov_base = buf;
        v.iov_len = size;
        return v;
    }

    inline io_vector io_vec(array_ref<void> buf)
    {
        return io_vec(buf.data(), buf.byte_size());
    }

    inline array_ref<void> io_vec_to_array_ref(io_vector v)
    {
        return array_ref<char>(static_cast<char*>(v.iov_base), v.iov_len);
    }

    inline size_t io_vector_array_size_sum(array_ref<const io_vector> array)
    {
        size_t result = 0;
        for( auto v : array )
            result += v.iov_len;
        return result;
    }

    inline bool io_vector_array_has_zero_size(array_ref<const io_vector> array)
    {
        for( auto v : array ) {
            if( v.iov_len > 0 )
                return false;
        }
        return true;
    }

    inline array_ref<io_vector> io_vector_array_advance(array_ref<io_vector> array, size_t size)
    {
        assert(array.size() > 0);
        assert(size <= io_vector_array_size_sum(array));

        size_t i = 0;
        while( size > 0 && array.at(i).iov_len <= size ) {
            size -= array.at(i).iov_len;
            ++i;
        }
        if( size > 0 ) {
            array[i].iov_base = static_cast<char*>(array[i].iov_base) + size;
            array[i].iov_len -= size;
        }
        return array.sub(i);
    }


    template< typename ...Args >
    struct temp_fail_retry {
        template< ssize_t (*IOFunction)(Args...) >
        static int apply(Args... args, ssize_t* result)
        {
            while( true ) {
                *result = IOFunction(std::forward<Args>(args)...);
                if( *result >= 0 )
                    return 0;
                if( errno != EINTR )
                    return errno;
            }
        }
    };


    enum ReadStatus {
        ReadNoError = 0,
        ReadWouldBlock = EAGAIN,
        ReadBadFd = EBADF,
        ReadBadBuffer = EFAULT,
        ReadBadRequest = EINVAL,
        ReadIOError = EIO,
        ReadIsDir = EISDIR
    };

    inline int read(int filedes, array_ref<void> buffer, ssize_t* result)
    {
        return temp_fail_retry<int, void*, size_t>::apply<::read>(filedes, buffer.data(), buffer.byte_size(), result);
    }

    inline int readv(int filedes, array_ref<const io_vector> iov, ssize_t* result)
    {
        return temp_fail_retry<int, const io_vector*, int>::apply<::readv>(filedes, iov.data(), iov.size(), result);
    }

    // Reading past the end of stream is treated as an IOError
    inline int read_all(int filedes, array_ref<void> buffer)
    {
        auto buf = buffer.array_static_cast<char>();
        while( buf.size() > 0 ) {
            const auto r = ::read(filedes, buf.data(), buf.size());
            if( r > 0 ) {
                buf = buf.sub(size_t(r));
                continue;
            }
            if( r == 0 )
                return ReadIOError;
            if( errno != EINTR )
                return errno;
        }
        return ReadNoError;
    }


    enum WriteStatus {
        WriteNoError = 0,
        WriteWouldBlock = EAGAIN,
        WriteClosedPipe = EPIPE,
        WriteNotConnected = EDESTADDRREQ,
        WriteQuotaExceeded = EDQUOT,
        WriteTooBig = EFBIG,
        WriteNoSpace = ENOSPC,
        WriteIOError = EIO,
        WriteBadFd = EBADF,
        WriteBadBuffer = EFAULT,
        WriteBadRequest = EINVAL
    };

    inline int write(int filedes, array_ref<const void> buffer, ssize_t* result)
    {
        return temp_fail_retry<int, const void*, size_t>::apply<::write>(filedes, buffer.data(), buffer.byte_size(), result);
    }

    inline int writev(int filedes, array_ref<const io_vector> iov, ssize_t* result)
    {
        return temp_fail_retry<int, const io_vector*, int>::apply<::writev>(filedes, iov.data(), iov.size(), result);
    }

    inline int write_all(int filedes, array_ref<const void> buffer)
    {
        auto buf = buffer.array_static_cast<char>();
        while( buf.size() > 0 ) {
            const auto r = ::write(filedes, buf.data(), buf.size());
            if( r > 0 ) {
                buf = buf.sub(size_t(r));
                continue;
            }
            if( r == 0 )
                return WriteIOError;
            if( errno != EINTR )
                return errno;
        }
        return WriteNoError;
    }

} // namespace io
} // namespace lbu

#endif // LIBLBU_IO_H

