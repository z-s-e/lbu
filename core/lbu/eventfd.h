/* Copyright 2015-2020 Zeno Sebastian Endemann <zeno.endemann@mailbox.org>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LIBLBU_EVENTFD_H
#define LIBLBU_EVENTFD_H

#include "lbu/io.h"
#include "lbu/unexpected.h"

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
        unique_fd fd;
        int status = OpenNoError;
    };

    inline open_result open(unsigned initval = 0, int flags = FlagsNone)
    {
        int filedes = ::eventfd(initval, flags);
        return open_result{ unique_fd(filedes), filedes < 0 ? errno : 0 };
    }

    inline int read(fd f, eventfd_t* dst)
    {
        return io::read(f, array_ref<char>(reinterpret_cast<char*>(dst), sizeof(*dst))).status;
    }

    inline int write(fd f, eventfd_t src)
    {
        return io::write(f, array_ref<char>(reinterpret_cast<char*>(&src), sizeof(src))).status;
    }


    // higher level wrapper that bail on unlikely errors

    inline unique_fd create(unsigned initval = 0, int flags = FlagsNone)
    {
        int filedes = ::eventfd(initval, flags);
        if( filedes < 0 )
            unexpected_system_error(errno);
        return unique_fd(filedes);
    }

    inline eventfd_t read_value(fd f)
    {
        eventfd_t result = 0;
        if( int err = read(f, &result); err != 0 && err != EAGAIN )
            lbu::unexpected_system_error(err);
        return result;
    }

    inline bool write_value(fd f, eventfd_t value)
    {
        int err = write(f, value);
        if( err != 0 && err != EAGAIN )
            lbu::unexpected_system_error(err);
        return err == 0;
    }

}
}

#endif
