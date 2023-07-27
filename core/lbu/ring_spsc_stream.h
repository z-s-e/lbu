/* Copyright 2015-2020 Zeno Sebastian Endemann <zeno.endemann@mailbox.org>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LIBLBU_RING_SPSC_STREAM_H
#define LIBLBU_RING_SPSC_STREAM_H

#include "lbu/abstract_stream.h"
#include "lbu/eventfd.h"
#include "lbu/fd.h"

#include <atomic>

namespace lbu {
namespace stream {

    struct ring_spsc_shared_data {
        std::atomic<uint32_t> producer_index;
        std::atomic<uint32_t> consumer_index;
        std::atomic<bool> producer_wake;
        std::atomic<bool> consumer_wake;
        std::atomic<bool> eos;

        ring_spsc_shared_data()
            : producer_index(0)
            , consumer_index(0)
            , producer_wake(false)
            , consumer_wake(true)
            , eos(false)
        {}

        static event_fd::open_result LIBLBU_EXPORT open_event_fd();
    };

    class ring_spsc {
    private:
        struct data {
            ring_spsc_shared_data* shared = {};
            uint32_t ring_size = 0;
            uint32_t segment_limit = DefaultRingSegmentLimit;
            uint32_t last_index = 0;
            fd filedes;
        };

    public:

        static constexpr uint32_t DefaultRingSegmentLimit = (16 * 1024);

        class input_stream : public abstract_input_stream {
        public:
            LIBLBU_EXPORT input_stream();
            input_stream(array_ref<void> buffer, fd event_fd, ring_spsc_shared_data* s)
                : input_stream()
            {
                reset(buffer, event_fd, s);
            }

            LIBLBU_EXPORT ~input_stream() override;

            void LIBLBU_EXPORT reset(array_ref<void> buffer, fd event_fd, ring_spsc_shared_data* s);

            uint32_t segment_size_limit() const { return d.segment_limit; }
            void set_segment_size_limit(uint32_t limit)
            {
                d.segment_limit = limit > 0 ? limit : 1;
            }

            fd event_fd() const { return d.filedes; }

        protected:
            ssize_t LIBLBU_EXPORT read_stream(array_ref<io::io_vector> buf_array, size_t required_read) override;
            array_ref<const void> LIBLBU_EXPORT get_read_buffer(Mode mode) override;

            input_stream(input_stream&&) = default;
            input_stream& operator=(input_stream&&) = default;

        private:
            array_ref<const void> next_buffer(Mode mode);
            bool update_buffer_size(ring_spsc_shared_data* shared, uint32_t consumer_index,
                                    uint32_t segment_limit, uint32_t ring_size);

            data d;
        };


        class output_stream : public abstract_output_stream {
        public:
            LIBLBU_EXPORT output_stream();
            output_stream(array_ref<void> buffer, fd event_fd, ring_spsc_shared_data* s)
                : output_stream()
            {
                reset(buffer, event_fd, s);
            }

            LIBLBU_EXPORT ~output_stream() override;

            bool LIBLBU_EXPORT set_end_of_stream();

            void LIBLBU_EXPORT reset(array_ref<void> buffer, fd event_fd, ring_spsc_shared_data* s);

            uint32_t segment_size_limit() const { return d.segment_limit; }
            void set_segment_size_limit(uint32_t limit)
            {
                d.segment_limit = limit > 0 ? limit : 1;
            }

            fd event_fd() const { return d.filedes; }

        protected:
            ssize_t LIBLBU_EXPORT write_stream(array_ref<io::io_vector> buf_array, Mode mode) override;
            array_ref<void> LIBLBU_EXPORT get_write_buffer(Mode mode) override;
            bool LIBLBU_EXPORT write_buffer_flush(Mode mode) override;

            output_stream(output_stream&&) = default;
            output_stream& operator=(output_stream&&) = default;

        private:
            array_ref<void> next_buffer(Mode mode);
            bool update_buffer_size(ring_spsc_shared_data* shared, uint32_t producer_index,
                                    uint32_t segment_limit, uint32_t ring_size);

            data d;
        };


        static void pair_streams(output_stream* out,
                                 input_stream* in,
                                 array_ref<void> buffer,
                                 fd event_fd,
                                 ring_spsc_shared_data* s,
                                 uint32_t segment_limit = DefaultRingSegmentLimit)
        {
            out->reset(buffer, event_fd, s);
            out->set_segment_size_limit(segment_limit);
            in->reset(buffer, event_fd, s);
            in->set_segment_size_limit(segment_limit);
        }
    };

    // convenience class that uses internally malloced buffer

    class ring_spsc_basic_controller {
    public:
        static constexpr uint32_t DefaultRingBufferSize = 4 * ring_spsc::DefaultRingSegmentLimit;

        explicit LIBLBU_EXPORT ring_spsc_basic_controller(uint32_t bufsize = DefaultRingBufferSize);

        LIBLBU_EXPORT ~ring_spsc_basic_controller();

        bool LIBLBU_EXPORT pair_streams(ring_spsc::output_stream* out, ring_spsc::input_stream* in,
                                        uint32_t segment_limit = ring_spsc::DefaultRingSegmentLimit);

        ring_spsc_basic_controller(const ring_spsc_basic_controller&) = delete;
        ring_spsc_basic_controller& operator=(const ring_spsc_basic_controller&) = delete;

    private:
        struct internal;
        internal* d;
    };

}
}

#endif
