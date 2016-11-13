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

#ifndef LIBLBU_FD_H
#define LIBLBU_FD_H

#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#include <utility>

namespace lbu {
namespace fd {

    /// unique_ptr like handle to file descriptors
    class unique_fd {
    public:
        unique_fd() = default;
        explicit unique_fd(int filedes) : fd(filedes) {}
        unique_fd(std::nullptr_t) {}

        unique_fd(unique_fd&& u)
        {
            reset(u.fd);
            u.fd = -1;
        }

        ~unique_fd() { reset(); }

        unique_fd& operator=(std::nullptr_t)
        {
            reset();
            return *this;
        }

        unique_fd& operator=(unique_fd&& u)
        {
            reset(u.fd);
            u.fd = -1;
            return *this;
        }

        explicit operator bool() const { return fd > -1; }

        int get() const { return fd; }
        int operator*() const { return get(); }

        int release()
        {
            int filedes = fd;
            fd = -1;
            return filedes;
        }

        void reset(int filedes = -1)
        {
            if( fd > -1 )
                ::close(fd);
            fd = filedes;
        }

        void swap(unique_fd& u)
        {
            std::swap(fd, u.fd);
        }

        unique_fd(const unique_fd&) = delete;
        unique_fd& operator=(const unique_fd&) = delete;

    private:
        int fd = -1;
    };


    inline bool is_cloexec(int filedes)
    {
        int f = fcntl(filedes, F_GETFD, 0);
        return (f >= 0) && (f & FD_CLOEXEC);
    }

    inline bool set_cloexec(int filedes, bool value)
    {
        int oldflags = fcntl(filedes, F_GETFD, 0);
        if( oldflags == -1 )
            return false;
        if( value )
            oldflags |= FD_CLOEXEC;
        else
            oldflags &= ~FD_CLOEXEC;
        return fcntl(filedes, F_SETFD, oldflags) != -1;
    }


    inline bool is_nonblock(int filedes)
    {
        int f = fcntl(filedes, F_GETFL, 0);
        return (f >= 0) && (f & O_NONBLOCK);
    }

    inline bool set_nonblock(int filedes, bool value)
    {
        int oldflags = fcntl(filedes, F_GETFL, 0);
        if( oldflags == -1 )
            return false;
        if( value )
            oldflags |= O_NONBLOCK;
        else
            oldflags &= ~O_NONBLOCK;
        return fcntl(filedes, F_SETFL, oldflags) != -1;
    }

} // namespace fd
} // namespace lbu

#endif // LIBLBU_FD_H

