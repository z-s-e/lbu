/* Copyright 2015-2020 Zeno Sebastian Endemann <zeno.endemann@mailbox.org>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LIBLBU_FILE_H
#define LIBLBU_FILE_H

#include "lbu/fd.h"

#include <limits>
#include <stdint.h>

namespace lbu {
namespace file {

    static constexpr uint64_t MaximumFileSize = std::numeric_limits<int64_t>::max();

    enum AccessFlags {
        AccessRead = O_RDONLY,
        AccessWrite = O_WRONLY,
        AccessReadWrite = O_RDWR
    };

    enum OpenFlags {
        FlagsCreate = O_CREAT,
        FlagsExclusive = O_EXCL,
        FlagsTruncate = O_TRUNC,
        FlagsNoFollowLink = O_NOFOLLOW,
        FlagsTmpFile = O_TMPFILE,
        FlagsPathOnly = O_PATH,
        FlagsCloExec = O_CLOEXEC
    };

    enum OperationFlags {
        FlagsNonBlock = O_NONBLOCK,
        FlagsAppend = O_APPEND,
        FlagsNoATime = O_NOATIME,
        FlagsDSync = O_DSYNC,
        FlagsSync = O_SYNC
    };

    enum OpenStatus {
        OpenNoError = 0,
        OpenAccessError = EACCES,
        OpenQuotaExhausted = EDQUOT,
        OpenAlreadyExists = EEXIST,
        OpenBadPathParameter = EFAULT,
        OpenInvalidOrUnsupportedFlags = EINVAL,
        OpenIsDirectory = EISDIR,
        OpenTooManySymLinks = ELOOP,
        OpenProcTooManyFds = EMFILE,
        OpenPathTooLong = ENAMETOOLONG,
        OpenSysTooManyFds = ENFILE,
        OpenNoDeviceCompat = ENODEV,
        OpenNoDevice = ENXIO,
        OpenFileNotFound = ENOENT,
        OpenKernelMemoryExhausted = ENOMEM,
        OpenNoFSSpace = ENOSPC,
        OpenIsNotDirectory = ENOTDIR,
        OpenNotSupported = EOPNOTSUPP,
        OpenFileTooBig = EOVERFLOW,
        OpenPermissionError = EPERM,
        OpenReadOnlyFS = EROFS,
        OpenExeBusy = ETXTBSY,
        OpenWouldBlock = EWOULDBLOCK
    };

    struct open_result {
        unique_fd f;
        int status = OpenNoError;
    };

    inline open_result open(const char *pathname, int flags, int mode = (S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP))
    {
        open_result r;
        int filedes;
        while( true ) {
            filedes = ::open(pathname, flags, mode);
            r.f.reset(fd(filedes));
            if( filedes >= 0 )
                return r;
            if( errno == EINTR )
                continue;
            r.status = errno;
            return r;
        }
    }


} // namespace file
} // namespace lbu

#endif // LIBLBU_FILE_H

