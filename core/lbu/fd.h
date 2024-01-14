/* Copyright 2015-2023 Zeno Sebastian Endemann <zeno.endemann@mailbox.org>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LIBLBU_FD_H
#define LIBLBU_FD_H

#include <cassert>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#include <utility>

namespace lbu {

    struct fd {
        int value = -1;

        fd() = default;
        explicit fd(int filedes) : value(filedes) {}
        // default copy ctor/assignment ok

        explicit operator bool() const { return value > -1; }

        void close()
        {
            int c = ::close(value);
            assert(c == 0 || errno != EBADF);
        }

        inline bool is_nonblock(int* error)
        {
            int f = ::fcntl(value, F_GETFL, 0);
            if( f == -1 )
                *error = errno;
            return (f != -1) && (f & O_NONBLOCK);
        }

        inline bool set_nonblock(bool nonblock, int* error)
        {
            int oldflags = ::fcntl(value, F_GETFL, 0);
            if( oldflags == -1 ) {
                *error = errno;
                return false;
            }
            if( ::fcntl(value, F_SETFL, nonblock ? (oldflags | O_NONBLOCK)
                                                 : (oldflags & (~O_NONBLOCK))) == -1 ) {
                *error = errno;
                return false;
            }
            return true;
        }

        inline bool is_cloexec(int* error)
        {
            int f = ::fcntl(value, F_GETFD, 0);
            if( f == -1 )
                *error = errno;
            return (f != -1) && (f & FD_CLOEXEC);
        }

        inline bool set_cloexec(bool cloexec, int* error)
        {
            int oldflags = ::fcntl(value, F_GETFD, 0);
            if( oldflags == -1 ) {
                *error = errno;
                return false;
            }
            if( ::fcntl(value, F_SETFD, cloexec ? (oldflags | FD_CLOEXEC)
                                                : (oldflags & (~FD_CLOEXEC))) == -1 ) {
                *error = errno;
                return false;
            }
            return true;
        }
    };

    /// unique_ptr like handle to file descriptors
    class unique_fd {
    public:
        unique_fd() = default;
        explicit unique_fd(fd f) : d(f) {}
        explicit unique_fd(int fildes) : d(fildes) {}
        unique_fd(std::nullptr_t) {}

        unique_fd(unique_fd&& u)
        {
            reset(u.d);
            u.d = {};
        }

        ~unique_fd() { reset(); }

        unique_fd& operator=(std::nullptr_t)
        {
            reset();
            return *this;
        }

        unique_fd& operator=(unique_fd&& u)
        {
            reset(u.d);
            u.d = {};
            return *this;
        }

        explicit operator bool() const { return bool(d); }

        fd get() const { return d; }
        fd operator*() const { return get(); }

        fd release()
        {
            fd f = d;
            d = {};
            return f;
        }

        void reset(fd f = {})
        {
            if( d )
                d.close();
            d = f;
        }

        void swap(unique_fd& u)
        {
            std::swap(d.value, u.d.value);
        }

        unique_fd(const unique_fd&) = delete;
        unique_fd& operator=(const unique_fd&) = delete;

    private:
        fd d;
    };

}

#endif
