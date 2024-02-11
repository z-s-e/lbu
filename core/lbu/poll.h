/* Copyright 2015-2020 Zeno Sebastian Endemann <zeno.endemann@mailbox.org>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LIBLBU_POLL_H
#define LIBLBU_POLL_H

#include "lbu/array_ref.h"
#include "lbu/fd.h"
#include "lbu/unexpected.h"

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

    inline void swap(unique_pollfd& lhs, unique_pollfd& rhs)
    {
        using std::swap;
        swap(lhs.d.fd, rhs.d.fd);
        swap(lhs.d.events, rhs.d.events);
        swap(lhs.d.revents, rhs.d.revents);
    }


    struct poll_result {
        int count;
        int status = StatusNoError;
    };

    inline poll_result poll(pollfd* fds, nfds_t nfds,
                            std::chrono::milliseconds timeout = NoTimeout)
    {
        poll_result r;
        r.count = ::poll(fds, nfds, timeout.count());
        if( r.count == -1 )
            r.status = errno;
        return r;
    }

    inline poll_result poll(array_ref<pollfd> fds,
                            std::chrono::milliseconds timeout = NoTimeout)
    {
        return poll(fds.data(), fds.size(), timeout);
    }

    inline constexpr struct timespec timespec_for_duration(std::chrono::nanoseconds ns)
    {
        struct timespec ts = {};
        ts.tv_nsec = ns.count() % std::nano::den;
        ts.tv_sec = ns.count() / std::nano::den;
        return ts;
    }

    inline poll_result ppoll(pollfd* fds, nfds_t nfds,
                             const struct timespec* timeout_ts = nullptr,
                             const sigset_t* sigmask = nullptr)
    {
        poll_result r;
        r.count = ::ppoll(fds, nfds, timeout_ts, sigmask);
        if( r.count == -1 )
            r.status = errno;
        return r;
    }

    inline poll_result ppoll(array_ref<pollfd> fds,
                             const struct timespec* timeout_ts = nullptr,
                             const sigset_t* sigmask = nullptr)
    {
        return lbu::poll::ppoll(fds.data(), fds.size(), timeout_ts, sigmask);
    }

    inline poll_result ppoll(pollfd* fds, nfds_t nfds,
                             std::chrono::nanoseconds timeout,
                             const sigset_t* sigmask = nullptr)
    {
        const auto ts = timespec_for_duration(timeout);
        return lbu::poll::ppoll(fds, nfds, &ts, sigmask);
    }

    inline poll_result ppoll(array_ref<pollfd> fds,
                             std::chrono::nanoseconds timeout, const sigset_t* sigmask = nullptr)
    {
        return ppoll(fds.data(), fds.size(), timeout, sigmask);
    }


    inline void wait_for_events(pollfd* fds, nfds_t nfds)
    {
        while( true ) {
            auto p = poll(fds, nfds);
            if( p.status == lbu::poll::StatusNoError )
                return;
            else if( p.status == lbu::poll::StatusPollInterrupted )
                continue;
            lbu::unexpected_system_error(p.status);
        }
    }

    inline void wait_for_events(array_ref<pollfd> fds) { wait_for_events(fds.data(), fds.size()); }

    inline void wait_for_event(pollfd p) { wait_for_events(&p, 1); }
    inline void wait_for_event(fd f, short flags) { wait_for_event(poll_fd(f, flags)); }

}
}

#endif
