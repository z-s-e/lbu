/* Copyright 2015-2020 Zeno Sebastian Endemann <zeno.endemann@mailbox.org>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LIBLBU_EVENTFD_H
#define LIBLBU_EVENTFD_H

#include "lbu/io.h"

#include <stdint.h>
#include <sys/eventfd.h>

namespace lbu {
namespace event_fd {

    static constexpr eventfd_t MaximumValue = std::numeric_limits<uint64_t>::max() - 1;

    enum Flags {
        FlagsNone = 0,
        FlagsCloExec = EFD_CLOEXEC,
        FlagsNonBlock = EFD_NONBLOCK,
        FlagsSemaphore = EFD_SEMAPHORE
    };

    enum OpenStatus {
        OpenNoError = 0,
        OpenUnsupportedFlags = EINVAL,
        OpenProcTooManyFds = EMFILE,
        OpenSysTooManyFds = ENFILE,
        OpenMountInodeDeviceFailed = ENODEV,
        OpenOutOfMemory = ENOMEM
    };

    enum ReadStatus {
        ReadNoError = 0,
        ReadWouldBlock = EAGAIN,
        ReadBadFd = EBADF
    };

    enum WriteStatus {
        WriteNoError = 0,
        WriteWouldBlock = EAGAIN,
        WriteBadValue = EINVAL,
        WriteBadFd = EBADF
    };

    struct open_result {
        unique_fd f;
        int status = OpenNoError;
    };

    inline open_result open(unsigned initval = 0, int flags = FlagsNone)
    {
        open_result r;
        int filedes = ::eventfd(initval, flags);
        r.f.reset(fd(filedes));
        if( ! r.f )
            r.status = errno;
        return r;
    }

    inline int read(fd f, eventfd_t* dst)
    {
        return io::read(f, array_ref<char>(reinterpret_cast<char*>(dst), sizeof(*dst))).status;
    }

    inline int write(fd f, eventfd_t src)
    {
        return io::write(f, array_ref<char>(reinterpret_cast<char*>(&src), sizeof(src))).status;
    }

}
}

#endif
