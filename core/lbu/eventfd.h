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

#ifndef LIBLBU_EVENTFD_H
#define LIBLBU_EVENTFD_H

#include "lbu/io.h"

#include <cerrno>
#include <stdint.h>
#include <sys/eventfd.h>
#include <unistd.h>

namespace lbu {
namespace event_fd {

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

    static const eventfd_t MaximumValue = std::numeric_limits<uint64_t>::max() - 1;

    inline int open(int* filedes, unsigned initval = 0, int flags = FlagsNone)
    {
        *filedes = ::eventfd(initval, flags);
        if( *filedes == -1 )
            return errno;
        return OpenNoError;
    }

    inline int read(int filedes, eventfd_t* dst)
    {
        ssize_t r;
        return io::read(filedes, array_ref<char>(reinterpret_cast<char*>(dst), sizeof(*dst)), &r);
    }

    inline int write(int filedes, eventfd_t src)
    {
        ssize_t r;
        return io::write(filedes, array_ref<char>(reinterpret_cast<char*>(&src), sizeof(src)), &r);
    }

} // namespace event_fd
} // namespace lbu

#endif // LIBLBU_EVENTFD_H

