/* Copyright 2015-2020 Zeno Sebastian Endemann <zeno.endemann@mailbox.org>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LIBLBU_FD_STREAM_H
#define LIBLBU_FD_STREAM_H

#include "lbu/abstract_stream.h"
#include "lbu/fd.h"
#include "lbu/io.h"

namespace lbu {
namespace stream {

    enum class FdBlockingState : uint8_t {
        Automatic,          // the stream will update the blocking state of the fd for each request accordingly
        AlwaysBlocking,     // the stream assumes the fd will always block and will never change its blocking state
                            // - read or write operation requesting non-blocking will generate an EINVAL error
        AlwaysNonBlocking   // same principle as above
    };


    class fd_input_stream : public abstract_input_stream {
    public:
        explicit LIBLBU_EXPORT fd_input_stream(array_ref<void> buffer,
                                               fd f = {},
                                               FdBlockingState b = FdBlockingState::Automatic);
        LIBLBU_EXPORT ~fd_input_stream() override;

        void LIBLBU_EXPORT set_descriptor(fd f, FdBlockingState b = FdBlockingState::Automatic);
        fd descriptor() const { return filedes; }

        int status() const { return err; }

        void* buffer_base() { return buffer_base_ptr; }
        uint32_t buffer_read_available() const { return buffer_available; }

    protected:
        ssize_t LIBLBU_EXPORT read_stream(array_ref<io::io_vector> buf_array, size_t required_read) override;
        array_ref<const void> LIBLBU_EXPORT get_read_buffer(Mode mode) override;

        fd_input_stream(fd_input_stream&&) = default;
        fd_input_stream& operator=(fd_input_stream&&) = default;

    private:
        FdBlockingState fd_blocking;
        fd filedes;
        uint32_t buffer_capacity;
        int err;
    };


    class fd_output_stream : public abstract_output_stream {
    public:
        explicit LIBLBU_EXPORT fd_output_stream(array_ref<void> buffer,
                                                fd f = {},
                                                FdBlockingState b = FdBlockingState::Automatic);
        LIBLBU_EXPORT ~fd_output_stream() override;

        void LIBLBU_EXPORT set_descriptor(fd f, FdBlockingState b = FdBlockingState::Automatic);
        fd descriptor() const { return filedes; }

        int status() const { return err; }

        void* buffer_base() { return buffer_base_ptr; }

    protected:
        ssize_t LIBLBU_EXPORT write_stream(array_ref<io::io_vector> buf_array, Mode mode) override;
        array_ref<void> LIBLBU_EXPORT get_write_buffer(Mode mode) override;
        bool LIBLBU_EXPORT write_buffer_flush(Mode mode) override;

        fd_output_stream(fd_output_stream&&) = default;
        fd_output_stream& operator=(fd_output_stream&&) = default;

    private:
        ssize_t write_fd(array_ref<io::io_vector> buf_array, Mode mode);
        bool buffer_flush(Mode mode);
        void reset_buffer();

        FdBlockingState fd_blocking;
        fd filedes;
        uint32_t buffer_capacity;
        uint32_t buffer_write_offset;
        int err;
    };


    // convenience classes that use malloced buffers

    class socket_stream_pair {
    public:
        explicit LIBLBU_EXPORT socket_stream_pair(uint32_t bufsize = DefaultBufferSize);
        LIBLBU_EXPORT socket_stream_pair(uint32_t bufsize_read, uint32_t bufsize_write);

        LIBLBU_EXPORT ~socket_stream_pair();

        unique_fd LIBLBU_EXPORT take_reset(unique_fd f = {}, FdBlockingState b = FdBlockingState::Automatic);

        abstract_input_stream* input_stream() { return &in; }
        int input_status() const { return in.status(); }

        abstract_output_stream* output_stream() { return &out; }
        int output_status() const { return out.status(); }

        fd descriptor() { return in.descriptor(); }

    private:
        fd_input_stream in;
        fd_output_stream out;
    };

    class managed_fd_output_stream {
    public:
        explicit LIBLBU_EXPORT managed_fd_output_stream(uint32_t bufsize = DefaultBufferSize);
        explicit managed_fd_output_stream(unique_fd f,
                                          FdBlockingState b = FdBlockingState::Automatic,
                                          uint32_t bufsize = DefaultBufferSize)
            : managed_fd_output_stream(bufsize)
        {
            reset(std::move(f), b);
        }
        LIBLBU_EXPORT ~managed_fd_output_stream();

        void LIBLBU_EXPORT reset(unique_fd f, FdBlockingState b = FdBlockingState::Automatic);

        abstract_output_stream* stream() { return &out; }
        fd descriptor() { return out.descriptor(); }
        int status() const { return out.status(); }

    private:
        fd_output_stream out;
    };

    class managed_fd_input_stream {
    public:
        explicit LIBLBU_EXPORT managed_fd_input_stream(uint32_t bufsize = DefaultBufferSize);
        explicit managed_fd_input_stream(unique_fd f,
                                         FdBlockingState b = FdBlockingState::Automatic,
                                         uint32_t bufsize = DefaultBufferSize)
            : managed_fd_input_stream(bufsize)
        {
            reset(std::move(f), b);
        }
        LIBLBU_EXPORT ~managed_fd_input_stream();

        void LIBLBU_EXPORT reset(unique_fd f, FdBlockingState b = FdBlockingState::Automatic);

        abstract_input_stream* stream() { return &in; }
        fd descriptor() { return in.descriptor(); }
        int status() const { return in.status(); }

    private:
        fd_input_stream in;
    };

}
}

#endif
