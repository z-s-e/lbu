/* Copyright 2015-2017 Zeno Sebastian Endemann <zeno.endemann@googlemail.com>
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

#ifndef LIBLBU_FD_STREAM_H
#define LIBLBU_FD_STREAM_H

#include "lbu/abstract_stream.h"
#include "lbu/fd.h"
#include "lbu/io.h"

namespace lbu {
namespace stream {

    enum class FdBlockingState : unsigned char {
        Unknown = 0,
        Blocking = 1,
        NonBlocking = 2
    };


    class fd_input_stream : public abstract_input_stream {
    public:
        explicit LIBLBU_EXPORT fd_input_stream(array_ref<void> buffer);
        LIBLBU_EXPORT ~fd_input_stream() override;

        LIBLBU_EXPORT void set_descriptor(int filedes, FdBlockingState b);
        int descriptor() const { return fd; }

        int status() const { return err; }

        char* buffer_base() { return bufferBase; }

    protected:
        LIBLBU_EXPORT ssize_t read_stream(array_ref<io::io_vector> buf_array, size_t required_read) override;
        LIBLBU_EXPORT array_ref<const void> get_read_buffer(Mode mode) override;

        fd_input_stream(fd_input_stream&&) = default;
        fd_input_stream& operator=(fd_input_stream&&) = default;

    private:
        FdBlockingState fdBlocking;
        int fd;
        uint32_t bufferMaxSize;
        int err;
    };


    class fd_output_stream : public abstract_output_stream {
    public:
        explicit LIBLBU_EXPORT fd_output_stream(array_ref<void> buffer);
        LIBLBU_EXPORT ~fd_output_stream() override;

        LIBLBU_EXPORT void set_descriptor(int filedes, FdBlockingState b);
        int descriptor() const { return fd; }

        int status() const { return err; }

        char* buffer_base() { return bufferBase; }

    protected:
        LIBLBU_EXPORT ssize_t write_stream(array_ref<io::io_vector> buf_array, Mode mode) override;
        LIBLBU_EXPORT array_ref<void> get_write_buffer(Mode mode) override;
        LIBLBU_EXPORT bool write_buffer_flush(Mode mode) override;

        fd_output_stream(fd_output_stream&&) = default;
        fd_output_stream& operator=(fd_output_stream&&) = default;

    private:
        ssize_t write_fd(array_ref<io::io_vector> buf_array, Mode mode);
        bool buffer_flush(Mode mode);
        void reset_buffer();

        FdBlockingState fdBlocking;
        int fd;
        uint32_t bufferMaxSize;
        uint32_t bufferWriteOffset;
        int err;
    };


    // convenience classes that use malloced buffers

    class socket_stream_pair {
    public:
        explicit LIBLBU_EXPORT socket_stream_pair(uint32_t bufsize = DefaultBufferSize);
        LIBLBU_EXPORT socket_stream_pair(uint32_t bufsize_read, uint32_t bufsize_write);

        LIBLBU_EXPORT ~socket_stream_pair();

        LIBLBU_EXPORT void reset(fd::unique_fd filedes = {}, FdBlockingState b = FdBlockingState::Unknown);

        LIBLBU_EXPORT fd::unique_fd take_reset(fd::unique_fd filedes = {}, FdBlockingState b = FdBlockingState::Unknown);

        abstract_input_stream* input_stream() { return &in; }
        int input_status() const { return in.status(); }

        abstract_output_stream* output_stream() { return &out; }
        int output_status() const { return out.status(); }

        int descriptor() { return in.descriptor(); }

    private:
        fd_input_stream in;
        fd_output_stream out;
    };

    class managed_fd_output_stream {
    public:
        explicit LIBLBU_EXPORT managed_fd_output_stream(uint32_t bufsize = DefaultBufferSize);
        explicit managed_fd_output_stream(fd::unique_fd filedes,
                                          FdBlockingState b = FdBlockingState::Unknown,
                                          uint32_t bufsize = DefaultBufferSize)
            : managed_fd_output_stream(bufsize)
        {
            reset(std::move(filedes), b);
        }
        LIBLBU_EXPORT ~managed_fd_output_stream();

        LIBLBU_EXPORT void reset(fd::unique_fd filedes, FdBlockingState b = FdBlockingState::Unknown);

        abstract_output_stream* stream() { return &out; }
        int descriptor() { return out.descriptor(); }
        int status() const { return out.status(); }

    private:
        fd_output_stream out;
    };

    class managed_fd_input_stream {
    public:
        explicit LIBLBU_EXPORT managed_fd_input_stream(uint32_t bufsize = DefaultBufferSize);
        explicit managed_fd_input_stream(fd::unique_fd filedes,
                                         FdBlockingState b = FdBlockingState::Unknown,
                                         uint32_t bufsize = DefaultBufferSize)
            : managed_fd_input_stream(bufsize)
        {
            reset(std::move(filedes), b);
        }
        LIBLBU_EXPORT ~managed_fd_input_stream();

        LIBLBU_EXPORT void reset(fd::unique_fd filedes, FdBlockingState b = FdBlockingState::Unknown);

        abstract_input_stream* stream() { return &in; }
        int descriptor() { return in.descriptor(); }
        int status() const { return in.status(); }

    private:
        fd_input_stream in;
    };

} // namespace stream
} // namespace lbu

#endif // LIBLBU_FD_STREAM_H
