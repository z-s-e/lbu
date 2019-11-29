/* Copyright 2015-2019 Zeno Sebastian Endemann <zeno.endemann@googlemail.com>
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

#ifndef LIBLBU_FD_H
#define LIBLBU_FD_H

#include "lbu/bit_ops.h"

#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#include <utility>

namespace lbu {

    struct fd {
        fd() = default; // default copy ctor/assignment ok
        explicit fd(int filedes) : value(filedes) {}

        explicit operator bool() const { return value > -1; }

        int value = -1;

        void close()
        {
            int c = ::close(value);
            assert(c == 0 || errno != EBADF);
        }

        inline bool is_nonblock()
        {
            int f = fcntl(value, F_GETFL, 0);
            return (f >= 0) && (f & O_NONBLOCK);
        }

        inline bool set_nonblock(bool value)
        {
            int oldflags = fcntl(value, F_GETFL, 0);
            if( oldflags == -1 )
                return false;
            return fcntl(value, F_SETFL, bit_ops::flag_set(oldflags, O_NONBLOCK, value)) != -1;
        }

        inline bool is_cloexec()
        {
            int f = fcntl(value, F_GETFD, 0);
            return (f >= 0) && (f & FD_CLOEXEC);
        }

        inline bool set_cloexec(bool value)
        {
            int oldflags = fcntl(value, F_GETFD, 0);
            if( oldflags == -1 )
                return false;
            return fcntl(value, F_SETFD, bit_ops::flag_set(oldflags, FD_CLOEXEC, value)) != -1;
        }
    };

    /// unique_ptr like handle to file descriptors
    class unique_fd {
    public:
        unique_fd() = default;
        explicit unique_fd(fd f) : data(f) {}
        explicit unique_fd(int fildes) : data(fildes) {}
        unique_fd(std::nullptr_t) {}

        unique_fd(unique_fd&& u)
        {
            reset(u.data);
            u.data = {};
        }

        ~unique_fd() { reset(); }

        unique_fd& operator=(std::nullptr_t)
        {
            reset();
            return *this;
        }

        unique_fd& operator=(unique_fd&& u)
        {
            reset(u.data);
            u.data = {};
            return *this;
        }

        explicit operator bool() const { return bool(data); }

        fd get() const { return data; }
        fd operator*() const { return get(); }

        fd release()
        {
            fd f = data;
            data = {};
            return f;
        }

        void reset(fd f = {})
        {
            if( data )
                data.close();
            data = f;
        }

        void swap(unique_fd& u)
        {
            std::swap(data.value, u.data.value);
        }

        unique_fd(const unique_fd&) = delete;
        unique_fd& operator=(const unique_fd&) = delete;

    private:
        fd data;
    };

} // namespace lbu

#endif // LIBLBU_FD_H

