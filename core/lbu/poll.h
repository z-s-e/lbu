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

#ifndef LIBLBU_POLL_H
#define LIBLBU_POLL_H

#include "lbu/array_ref.h"
#include "lbu/fd.h"

#include <cerrno>
#include <chrono>
#include <poll.h>

namespace lbu {
namespace poll {

    static const std::chrono::milliseconds NoTimeout = std::chrono::milliseconds(-1);

    enum Flags {
        FlagsReadReady = POLLIN,
        FlagsWriteReady = POLLOUT,
        FlagsPriorityReadReady = POLLPRI,
        FlagsStreamSockHangup = POLLRDHUP
    };

    enum ResultFlags {
        FlagsResultError = POLLERR,
        FlagsResultHangup = POLLHUP,
        FlagsResultInvalid = POLLNVAL
    };

    enum Status {
        StatusNoError = 0,
        StatusPollInterrupted = EINTR,
        StatusBadFdArray = EFAULT,
        StatusTooManyFds = EINVAL,
        StatusOutOfMemory = ENOMEM
    };


    inline pollfd poll_fd(fd f = {}, short flags = 0)
    {
        pollfd d;
        d.fd = f.value;
        d.events = flags;
        d.revents = 0;
        return d;
    }

    inline bool pollfd_is_ignored(const pollfd* pfd)
    {
        return  pfd->fd < 0;
    }

    inline void pollfd_toggle_ignored(pollfd* pfd)
    {
        pfd->fd = -(pfd->fd) - 1;
    }

    inline short flags(const pollfd* pfd)
    {
        return pfd->events;
    }

    inline void set_flags(pollfd* pfd, short flags)
    {
        pfd->events = flags;
    }

    inline bool has_events(const pollfd* pfd)
    {
        return pfd->revents != 0;
    }

    inline short events(const pollfd* pfd)
    {
        return pfd->revents;
    }

    inline void clear_events(pollfd* pfd)
    {
        pfd->revents = 0;
    }


    class unique_pollfd {
    public:
        unique_pollfd()
        {
            d.fd = -1;
            d.events = 0;
            d.revents = 0;
        }

        unique_pollfd(fd f, int flags)
        {
            d.fd = f.value;
            d.events = flags;
            d.revents = 0;
        }

        unique_pollfd(const unique_pollfd&) = delete;
        unique_pollfd& operator=(const unique_pollfd&) = delete;

        friend void swap(unique_pollfd& lhs, unique_pollfd& rhs);

        unique_pollfd(unique_pollfd&& other)
        {
            d = other.d;
            other.d.fd = -1;
        }

        unique_pollfd& operator=(unique_pollfd&& other)
        {
            reset_descriptor(other.descriptor());
            other.d.fd = -1;
            return *this;
        }

        ~unique_pollfd()
        {
            if( d.fd >= 0 )
                ::close(d.fd);
        }

        fd descriptor() const { return fd(d.fd); }

        void reset_descriptor(fd f)
        {
            if( descriptor() )
                descriptor().close();
            d.fd = f.value;
        }

        fd release_descriptor()
        {
            fd f(d.fd);
            d.fd = -1;
            return f;
        }

        short flags() const { return d.events; }
        void set_flags(short flags) { d.events = flags; }

        void reset(fd f, short flags) { reset_descriptor(f); set_flags(flags); }

        bool has_events() const { return d.revents != 0; }
        short events() const { return d.revents; }
        void clear_events() { d.revents = 0; }

        pollfd* as_pollfd() { return &d; }

    private:
        pollfd d;
    };
    static_assert(sizeof(unique_pollfd) == sizeof(pollfd), "unique_entry must have pollfd layout");

    inline void swap(unique_pollfd& lhs, unique_pollfd& rhs)
    {
        using std::swap;
        swap(lhs.d.fd, rhs.d.fd);
        swap(lhs.d.events, rhs.d.events);
        swap(lhs.d.revents, rhs.d.revents);
    }


    inline int poll(pollfd* fds, nfds_t nfds, int* count,
                    std::chrono::milliseconds timeout = NoTimeout)
    {
        *count = ::poll(fds, nfds, timeout.count());
        return *count == -1 ? errno : StatusNoError;
    }

    inline int poll(array_ref<pollfd> fds, int* count,
                    std::chrono::milliseconds timeout = NoTimeout)
    {
        return poll(fds.data(), fds.size(), count, timeout);
    }

    inline int ppoll(pollfd* fds, nfds_t nfds, int* count,
                     const struct timespec* timeout_ts = nullptr,
                     const sigset_t* sigmask = nullptr)
    {
        *count = ::ppoll(fds, nfds, timeout_ts, sigmask);
        return *count == -1 ? errno : StatusNoError;
    }

    inline int ppoll(array_ref<pollfd> fds, int* count,
                     const struct timespec* timeout_ts = nullptr,
                     const sigset_t* sigmask = nullptr)
    {
        return ppoll(fds.data(), fds.size(), count, timeout_ts, sigmask);
    }

    inline int ppoll(pollfd* fds, nfds_t nfds, int* count,
                     std::chrono::nanoseconds timeout,
                     const sigset_t* sigmask = nullptr)
    {
        struct timespec ts;
        ts.tv_nsec = timeout.count() % std::nano::den;
        ts.tv_sec = timeout.count() / std::nano::den;
        return ppoll(fds, nfds, count, &ts, sigmask);
    }

    inline int ppoll(array_ref<pollfd> fds, int* count,
                     std::chrono::nanoseconds timeout, const sigset_t* sigmask = nullptr)
    {
        return ppoll(fds.data(), fds.size(), count, timeout, sigmask);
    }

    inline bool wait_for_event(fd f, short flags)
    {
        auto p = poll_fd(f, flags);
        while( true ) {
            int c = ::poll(&p, 1, NoTimeout.count());
            if( c == 1 )
                return (flags & p.revents);
            if( c == -1 && errno == EINTR)
                continue;
            return false;
        }
    }

    // TODO: add epoll, flush_fd?

} // namespace poll
} // namespace lbu

#endif // LIBLBU_POLL_H

