/* Copyright 2015-2019 Zeno Sebastian Endemann <zeno.endemann@googlemail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LIBLBU_IO_H
#define LIBLBU_IO_H

#include "lbu/array_ref.h"
#include "lbu/fd.h"

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


    struct io_result {
        ssize_t size;
        int status = 0; // == ReadNoError == WriteNoError
    };

    template< typename ...Args >
    struct temp_fail_retry {
        template< ssize_t (*IOFunction)(int, Args...) >
        static io_result apply(int filedes, Args... args)
        {
            io_result r;
            while( true ) {
                r.size = IOFunction(filedes, args...);
                if( r.size >= 0 )
                    return r;
                if( errno != EINTR ) {
                    r.status = errno;
                    return r;
                }
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

    inline io_result read(fd f, array_ref<void> buffer)
    {
        return temp_fail_retry<void*, size_t>::apply<::read>(f.value, buffer.data(), buffer.byte_size());
    }

    inline io_result readv(fd f, array_ref<const io_vector> iov)
    {
        return temp_fail_retry<const io_vector*, int>::apply<::readv>(f.value, iov.data(), iov.size());
    }

    // Reading past the end of stream is treated as an IOError
    inline int read_all(fd f, array_ref<void> buffer)
    {
        auto buf = buffer.array_static_cast<char>();
        while( buf.size() > 0 ) {
            const auto r = ::read(f.value, buf.data(), buf.size());
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

    inline io_result write(fd f, array_ref<const void> buffer)
    {
        return temp_fail_retry<const void*, size_t>::apply<::write>(f.value, buffer.data(), buffer.byte_size());
    }

    inline io_result writev(fd f, array_ref<const io_vector> iov)
    {
        return temp_fail_retry<const io_vector*, int>::apply<::writev>(f.value, iov.data(), iov.size());
    }

    inline int write_all(fd f, array_ref<const void> buffer)
    {
        auto buf = buffer.array_static_cast<char>();
        while( buf.size() > 0 ) {
            const auto r = ::write(f.value, buf.data(), buf.size());
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

