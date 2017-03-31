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

#ifndef LIBLBU_RING_SPSC_STREAM_H
#define LIBLBU_RING_SPSC_STREAM_H

#include "lbu/abstract_stream.h"

#include <atomic>

namespace lbu {
namespace stream {

    struct ring_spsc_shared_data {
        ring_spsc_shared_data()
            : producer_index(0)
            , consumer_index(0)
            , producer_wake(false)
            , consumer_wake(true)
        {}

        LIBLBU_EXPORT static bool open_event_fd(int* event_fd);

        std::atomic<uint32_t> producer_index;
        std::atomic<uint32_t> consumer_index;
        std::atomic<bool> producer_wake;
        std::atomic<bool> consumer_wake;
    };

    class ring_spsc {
    private:
        struct data {
            uint32_t bufferMaxSize = 0;
            ring_spsc_shared_data* shared = {};
            uint32_t bufferLimit = DefaultBufferSegmentLimit;
            uint32_t lastIndex = 0;
            int fd = -1;
        };

    public:

        static constexpr uint32_t DefaultBufferSegmentLimit = (1 << 15);

        class input_stream : public abstract_input_stream {
        public:
            LIBLBU_EXPORT input_stream();
            input_stream(array_ref<void> buffer, int event_fd, ring_spsc_shared_data* shared)
                : input_stream()
            {
                reset(buffer, event_fd, shared);
            }

            LIBLBU_EXPORT ~input_stream() override;

            LIBLBU_EXPORT void reset(array_ref<void> buffer, int event_fd, ring_spsc_shared_data* s);

            uint32_t buffer_segment_limit() const { return d.bufferLimit; }
            void set_buffer_segment_limit(uint32_t limit)
            {
                d.bufferLimit = limit > 0 ? limit : 1;
            }

            int event_fd() const { return d.fd; }

        protected:
            LIBLBU_EXPORT ssize_t read_stream(array_ref<io::io_vector> buf_array, size_t required_read) override;
            LIBLBU_EXPORT array_ref<const void> get_read_buffer(Mode mode) override;

            input_stream(input_stream&&) = default;
            input_stream& operator=(input_stream&&) = default;

        private:
            array_ref<const void> next_buffer(Mode mode);
            void update_buffer_state(uint32_t producer_index, uint32_t consumer_index);

            data d;
        };


        class output_stream : public abstract_output_stream {
        public:
            LIBLBU_EXPORT output_stream();
            output_stream(array_ref<void> buffer, int event_fd, ring_spsc_shared_data* shared)
                : output_stream()
            {
                reset(buffer, event_fd, shared);
            }

            LIBLBU_EXPORT ~output_stream() override;

            LIBLBU_EXPORT void reset(array_ref<void> buffer, int event_fd, ring_spsc_shared_data* s);

            uint32_t buffer_segment_limit() const { return d.bufferLimit; }
            void set_buffer_segment_limit(uint32_t limit)
            {
                d.bufferLimit = limit > 0 ? limit : 1;
            }

            int event_fd() const { return d.fd; }

        protected:
            LIBLBU_EXPORT ssize_t write_stream(array_ref<io::io_vector> buf_array, Mode mode) override;
            LIBLBU_EXPORT array_ref<void> get_write_buffer(Mode mode) override;
            LIBLBU_EXPORT bool write_buffer_flush(Mode mode) override;

            output_stream(output_stream&&) = default;
            output_stream& operator=(output_stream&&) = default;

        private:
            array_ref<void> next_buffer(Mode mode);
            void update_buffer_state(uint32_t producer_index, uint32_t consumer_index);

            data d;
        };


        static void pair_streams(output_stream* out, input_stream* in,
                                 array_ref<void> buffer,
                                 int event_fd,
                                 ring_spsc_shared_data* s,
                                 uint32_t buffer_limit = DefaultBufferSegmentLimit)
        {
            out->reset(buffer, event_fd, s);
            out->set_buffer_segment_limit(buffer_limit);
            in->reset(buffer, event_fd, s);
            in->set_buffer_segment_limit(buffer_limit);
        }
    };

    // convenience class that uses internally malloced buffer

    class ring_spsc_basic_controller {
    public:
        static const uint32_t DefaultRingBufferSize = 2 * ring_spsc::DefaultBufferSegmentLimit;

        explicit LIBLBU_EXPORT ring_spsc_basic_controller(uint32_t bufsize = DefaultRingBufferSize);

        LIBLBU_EXPORT ~ring_spsc_basic_controller();

        LIBLBU_EXPORT bool pair_streams(ring_spsc::output_stream* out,
                                        ring_spsc::input_stream* in,
                                        uint32_t buffer_limit = ring_spsc::DefaultBufferSegmentLimit);

        ring_spsc_basic_controller(const ring_spsc_basic_controller&) = delete;
        ring_spsc_basic_controller& operator=(const ring_spsc_basic_controller&) = delete;

    private:
        struct internal;
        internal* d;
    };

} // namespace stream
} // namespace lbu

#endif // LIBLBU_RING_SPSC_STREAM_H