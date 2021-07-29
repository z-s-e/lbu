/* Copyright 2015-2016 Zeno Sebastian Endemann <zeno.endemann@mailbox.org>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LIBLBU_BYTE_BUFFER_STREAM_H
#define LIBLBU_BYTE_BUFFER_STREAM_H

#include "lbu/abstract_stream.h"
#include "lbu/byte_buffer.h"

namespace lbu {
namespace stream {

    class byte_buffer_input_stream : public abstract_input_stream {
    public:
        explicit LIBLBU_EXPORT byte_buffer_input_stream(array_ref<void> buf = {});
        LIBLBU_EXPORT ~byte_buffer_input_stream() override;

        LIBLBU_EXPORT void reset(array_ref<void> buf);

    protected:
        LIBLBU_EXPORT ssize_t read_stream(array_ref<io::io_vector> buf_array, size_t required_read) override;
        LIBLBU_EXPORT array_ref<const void> get_read_buffer(Mode mode) override;

        byte_buffer_input_stream(byte_buffer_input_stream&&) = default;
        byte_buffer_input_stream& operator=(byte_buffer_input_stream&&) = default;
    };


    class byte_buffer_output_stream : public abstract_output_stream {
    public:
        LIBLBU_EXPORT byte_buffer_output_stream();
        LIBLBU_EXPORT ~byte_buffer_output_stream() override;

        LIBLBU_EXPORT void reset(byte_buffer&& buf);
        byte_buffer release_reset(byte_buffer&& buf = {})
        {
            auto b = std::move(buffer);
            reset(std::move(buf));
            return b;
        }

    protected:
        LIBLBU_EXPORT ssize_t write_stream(array_ref<io::io_vector> buf_array, Mode mode) override;
        LIBLBU_EXPORT array_ref<void> get_write_buffer(Mode mode) override;
        LIBLBU_EXPORT bool write_buffer_flush(Mode mode) override;

        byte_buffer_output_stream(byte_buffer_output_stream&&) = default;
        byte_buffer_output_stream& operator=(byte_buffer_output_stream&&) = default;

    private:
        void sync_state();

        byte_buffer buffer;
    };

} // namespace stream
} // namespace lbu

#endif // LIBLBU_BYTE_BUFFER_STREAM_H
