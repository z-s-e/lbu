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

#ifndef LIBLBU_PIPE_H
#define LIBLBU_PIPE_H

#include <cerrno>
#include <fcntl.h>
#include <limits.h>
#include <unistd.h>

namespace lbu {
namespace pipe {

    enum Flags {
        FlagsNone = 0,
        FlagsCloExec = O_CLOEXEC,
        FlagsNonBlock = O_NONBLOCK
    };

    enum Status {
        StatusNoError = 0,
        StatusInvalidFlags = EINVAL,
        StatusProcTooManyFds = EMFILE,
        StatusSysTooManyFds = ENFILE
    };

    static const int AtomicWriteSize = PIPE_BUF;

    inline int open(int* read_filedes, int* write_filedes, int flags = FlagsNone)
    {
        int fds[2];
        int s = pipe2(fds, flags);
        if( s == 0 ) {
            *read_filedes = fds[0];
            *write_filedes = fds[1];
            return StatusNoError;
        }
        return errno;
    }

} // namespace pipe
} // namespace lbu

#endif // LIBLBU_PIPE_H

