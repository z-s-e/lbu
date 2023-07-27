/* Copyright 2015-2020 Zeno Sebastian Endemann <zeno.endemann@mailbox.org>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LIBLBU_PIPE_H
#define LIBLBU_PIPE_H

#include "lbu/fd.h"

#include <limits.h>

namespace lbu {
namespace pipe {

    static constexpr int AtomicWriteSize = PIPE_BUF;

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

    struct open_result {
        unique_fd read_fd;
        unique_fd write_fd;
        int status = StatusNoError;
    };

    inline open_result open(int flags = FlagsNone)
    {
        open_result r;
        int fds[2];
        int s = pipe2(fds, flags);
        r.read_fd.reset(fd(fds[0]));
        r.write_fd.reset(fd(fds[1]));
        if( s != 0 )
            r.status = errno;
        return r;
    }

}
}

#endif
