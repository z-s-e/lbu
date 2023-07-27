/* Copyright 2015-2020 Zeno Sebastian Endemann <zeno.endemann@mailbox.org>
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

        inline bool is_nonblock()
        {
            int f = ::fcntl(value, F_GETFL, 0);
            return (f >= 0) && (f & O_NONBLOCK);
        }

        inline bool set_nonblock(bool value)
        {
            int oldflags = ::fcntl(value, F_GETFL, 0);
            if( oldflags == -1 )
                return false;
            return ::fcntl(value, F_SETFL, value ? (oldflags | O_NONBLOCK)
                                                 : (oldflags & (~O_NONBLOCK))) != -1;
        }

        inline bool is_cloexec()
        {
            int f = ::fcntl(value, F_GETFD, 0);
            return (f >= 0) && (f & FD_CLOEXEC);
        }

        inline bool set_cloexec(bool value)
        {
            int oldflags = ::fcntl(value, F_GETFD, 0);
            if( oldflags == -1 )
                return false;
            return ::fcntl(value, F_SETFD, value ? (oldflags | FD_CLOEXEC)
                                                 : (oldflags & (~FD_CLOEXEC))) != -1;
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
