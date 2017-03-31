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

#ifndef LIBLBU_FILE_H
#define LIBLBU_FILE_H

#include <cerrno>
#include <fcntl.h>
#include <unistd.h>

namespace lbu {
namespace file {

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

    inline int open(int* filedes, const char *pathname, int flags, int mode = (S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP))
    {
        int fd;
        while( true ) {
            fd = ::open(pathname, flags, mode);
            *filedes = fd;
            if( fd >= 0 )
                return OpenNoError;
            if( errno == EINTR )
                continue;
            return errno;
        }
    }


    enum class SeekOrigin {
        Start = SEEK_SET,
        CurrentPosition = SEEK_CUR,
        End = SEEK_END
    };

    enum SeekError {
        SeekNoError = 0,
        SeekBadFd = EBADF,
        SeekBadRequest = EINVAL,
        SeekOffsetOverflow = EOVERFLOW,
        SeekUnsupported = ESPIPE
    };

    inline int seek(int filedes, off_t offset, SeekOrigin whence = SeekOrigin::CurrentPosition)
    {
        if( ::lseek(filedes, offset, static_cast<int>(whence)) == off_t(-1) )
            return errno;
        return SeekNoError;
    }

    inline int seek(int filedes, off_t offset, off_t* result, SeekOrigin whence = SeekOrigin::CurrentPosition)
    {
        *result = ::lseek(filedes, offset, static_cast<int>(whence));
        if( *result == off_t(-1) )
            return errno;
        return SeekNoError;
    }

} // namespace file
} // namespace lbu

#endif // LIBLBU_FILE_H
