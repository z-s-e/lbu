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

#ifndef LIBLBU_EVENTFD_H
#define LIBLBU_EVENTFD_H

#include "lbu/io.h"

#include <cerrno>
#include <stdint.h>
#include <sys/eventfd.h>
#include <unistd.h>

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

} // namespace event_fd
} // namespace lbu

#endif // LIBLBU_EVENTFD_H

