/* Copyright 2015-2024 Zeno Sebastian Endemann <zeno.endemann@mailbox.org>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LIBLBU_ABSTRACT_STREAM_H
#define LIBLBU_ABSTRACT_STREAM_H

#include "lbu/array_ref.h"
#include "lbu/lbu_global.h"
#include "lbu/io.h"

#include <cstring>

// A stream abstraction similar to the C FILE streams or std::iostream, but with
// many different design decisions. The design goals are
// - Support zero copy and scatter gather io.
// - Minimize optional API: streams here are either input or output sequential
//   streams, there are no runtime queries needed (whether a stream is input or
//   output or both, whether it is seekable or not etc) when using the standard
//   read/write interface. The only part of the API where subclasses can behave
//   differently are about performance optimized versions of these basic
//   operation (zero copy resp. scatter gather io).
// - No seekable concept: when seeking is needed I believe instead of bolting an
//   optional seek method on the stream API one should use a different, better
//   suited abstraction altogether and keep the stream abstraction simple.
// - No polymorphic close method: The way to properly close a stream is highly
//   dependent on the implementation and might require more complex (possibly
//   asynchronous) logic than what a simple close() method could abstract.
// - Optimize for the common case: for better performance streams are usually
//   buffered and in the common case the buffer is not empty resp. full.
//   For that case read resp. write operations are basically a memcpy and require
//   no virtual dispatch (and can even be inlined).
// - Be explicit about when the user wants blocking or nonblocking behavior: there
//   are many bugs relating to the blocking behavior being implicit and depending
//   on the runtime state of the underlying primitives. I believe having the mode
//   explicit should prevent most of these.
// - No internal thread synchronization: In most cases this is just unneeded
//   overhead and when it is needed it is trivial to add e.g. a mutex wrapper.

namespace lbu {
namespace stream {

    enum class InternalBuffer {
        No = 0,
        Yes = 1
    };

    enum class Mode {
        Blocking,
        NonBlocking
    };


namespace detail {

    class abstract_stream_base {
    protected:
        abstract_stream_base(InternalBuffer internal_buffer)
            : manages_buffer(internal_buffer == InternalBuffer::Yes) {}

        bool buffer_management() const { return manages_buffer; }

        array_ref<char> current_buffer()
        {
            return {buffer_base_ptr + buffer_offset, buffer_available};
        }

        void advance(size_t count)
        {
            assert(count <= buffer_available);
            buffer_offset += count;
            buffer_available -= count;
        }

        char* buffer_base_ptr = {};
        uint32_t buffer_offset = 0;
        uint32_t buffer_available = 0;

        enum StatusFlags {
            StatusError = 1<<0,
            StatusEndOfStream = 1<<1
        };
        uint8_t status_flags = 0;

    private:
        bool manages_buffer;
    };

}


    class abstract_input_stream : protected detail::abstract_stream_base {
    public:
        virtual LIBLBU_EXPORT ~abstract_input_stream();

        /// \brief Read from the stream into buf.
        ///
        /// If \p mode is Blocking, the function returns \p size on success, a nonnegative value less
        /// than \p size when the stream ends before \p size characters are read, or a negative value
        /// on a stream error.
        ///
        /// If \p mode is NonBlocking, the function may read less than \p size characters without
        /// error or end of stream (possibly 0). A negative return value signals a stream error.
        ///
        /// When the input stream is at the end, calls to this method will continue to return 0 without
        /// any error.
        ///
        /// \p size may be 0, and unless a stream error is detected, will return 0 without causing any
        /// errors. \p size may not exceed `std::numeric_limits<ssize_t>::max()`.
        ssize_t read(void* buf, size_t size, Mode mode)
        {
            if( buffer_available >= size && buffer_available > 0 ) {
                assert(manages_buffer());
                assert(buffer_base_ptr != nullptr);
                std::memcpy(buf, buffer_base_ptr + buffer_offset, size);
                advance_buffer(size);
                return ssize_t(size);
            }
            auto b = io::io_vec(static_cast<char*>(buf), size);
            return read_stream(array_ref_one_element(&b), mode == Mode::Blocking ? size : 0);
        }

        /// \brief True iff the stream manages internal buffer.
        ///
        /// Note that the internal buffer can already be the stream destination (e.g. a mmapped
        /// region of a file is also called an internal buffer here).
        ///
        /// Subclasses set this at construction time and cannot change it for the object's
        /// lifetime.
        bool manages_buffer() const { return buffer_management(); }

        /// \brief Return a ref to the internal buffer for zero copy access.
        ///
        /// Only call this after checking availabilty with `manages_buffer`.
        ///
        /// If \p mode is Blocking, an empty (invalid) array_ref indicates either a stream error or
        /// end of stream, otherwise a valid ref is returned. In NonBlocking mode an invalid ref
        /// may also be returned when no data is available without blocking.
        ///
        /// If a valid ref is returned, it will point to a buffer containing the next data in the
        /// stream. Once done with reading, one must call `advance_buffer` resp. `advance_whole_buffer`
        /// to move the internal read position forward.
        ///
        /// Calling `read` invalidates any ref previously returned by this method.
        ///
        /// Do not assume any particular alignment of the returned buffer or that all buffers
        /// returned from here are part of one continuous memory location. Subclasses are allowed
        /// to manage multiple seperate unaligned buffers of unspecific size internally.
        ///
        /// Writing into the internal buffer is not allowed.
        array_ref<const void> get_buffer(Mode mode)
        {
            assert(manages_buffer());
            if( buffer_available > 0 )
                return current_buffer();
            return get_read_buffer(mode);
        }

        /// \brief Move the internal buffer position forward by count characters.
        void advance_buffer(size_t count) { advance(count); }

        /// \brief Move the internal buffer position to the buffer's end.
        void advance_whole_buffer() { advance(buffer_available); }

        /// \brief Directly read from the stream.
        ///
        /// This method may only be called if `manages_buffer` returns false.
        ///
        /// For streams that don't have internal buffers this method can be used to add
        /// more efficient custom external buffer handling.
        ///
        /// When \p required_read is greater than 0 the method is allowed to modify the
        /// elements of the \p buf_array to avoid the need to make a copy of the array.
        ///
        /// On success the return value is the total amount of characters read (which
        /// is greater or equal to \p required_read). Otherwise a stream error occured and a
        /// negative value is returned.
        ///
        /// The combined sum of read requests (i.e. `io_vector_array_size_sum(buf_array)`)
        /// must be at least \p required_read and may not exceed `std::numeric_limits<ssize_t>::max()`.
        ///
        /// Provided that \p required_read is 0, \p buf_array may be empty or have a combined
        /// sum of 0 without causing any errors.
        ssize_t direct_read(array_ref<io::io_vector> buf_array, size_t required_read)
        {
            assert( ! manages_buffer());
            return read_stream(buf_array, required_read);
        }

        bool has_error() const { return (status_flags & StatusError); }
        bool at_end() const { return (status_flags & StatusEndOfStream); }

        abstract_input_stream(const abstract_input_stream&) = delete;
        abstract_input_stream& operator=(const abstract_input_stream&) = delete;

    protected:
        abstract_input_stream(InternalBuffer internal_buffer) : abstract_stream_base(internal_buffer) {}

        abstract_input_stream(abstract_input_stream&&) = default;
        abstract_input_stream& operator=(abstract_input_stream&&) = default;

        /// A note for implementing this virtual; for buffered streams the `read` method may call this
        /// with a \p required_read value past the end of stream - this should not generate an error,
        /// but conform to the documented `read` behavior. For unbuffered streams, a required read past
        /// the end shall result in a stream error.
        virtual ssize_t read_stream(array_ref<io::io_vector> buf_array, size_t required_read) = 0;
        virtual array_ref<const void> LIBLBU_EXPORT get_read_buffer(Mode mode);
    };


    class abstract_output_stream : protected detail::abstract_stream_base {
    public:
        /// \brief Destructor.
        ///
        /// Note that this does not call flush_buffer, it is the user's responsibility
        /// to properly flush all buffers before the destructor is run.
        ///
        /// Rationale: flushing is an operation that can fail or block, it should not be
        /// called implicitly.
        virtual LIBLBU_EXPORT ~abstract_output_stream();

        /// \brief Write from buf to the stream.
        ///
        /// If \p mode is Blocking, the function returns \p size on success or a negative value
        /// on a stream error.
        ///
        /// If \p mode is NonBlocking, the function may write less than \p size characters without
        /// error (possibly 0). A negative return value signals a stream error.
        ///
        /// \p size may be 0, and unless a stream error is detected, will return 0 without causing any
        /// errors. \p size may not exceed `std::numeric_limits<ssize_t>::max()`.
        ssize_t write(const void* buf, size_t size, Mode mode)
        {
            if( buffer_available >= size && buffer_available > 0 ) {
                assert(manages_buffer());
                assert(buffer_base_ptr != nullptr);
                std::memcpy(buffer_base_ptr + buffer_offset, buf, size);
                advance_buffer(size);
                return ssize_t(size);
            }
            auto b = io::io_vec(const_cast<void*>(buf), size);
            return write_stream(array_ref_one_element(&b), mode);
        }

        /// \brief True iff the stream manages internal buffer.
        ///
        /// Note that the internal buffer can already be the stream destination (e.g. a mmapped
        /// region of a file is also called an internal buffer here).
        ///
        /// Subclasses set this at construction time and cannot change it for the object's
        /// lifetime.
        bool manages_buffer() const { return buffer_management(); }

        /// \brief Flush internal buffer.
        ///
        /// Returns true when all internal buffer have been flushed. If \p mode is Blocking, returning
        /// false indicates a stream error. In NonBlocking mode, the method may return false even
        /// when no error occured (but flushing the internal buffers would block).
        bool flush_buffer(Mode mode = Mode::Blocking)
        {
            if( ! manages_buffer() )
                return true;
            return write_buffer_flush(mode);
        }

        /// \brief Return a ref to the internal buffer for zero copy access.
        ///
        /// Only call this after checking availabilty with `manages_buffer`.
        ///
        /// If \p mode is Blocking, an empty (invalid) array_ref indicates a stream error, otherwise a
        /// valid ref is returned. In NonBlocking mode an invalid ref may be returned even if no
        /// error occured.
        ///
        /// If a valid ref is returned, it will point to a buffer one can write into.
        /// Once done with writing, one must call `advance_buffer` resp. `advance_whole_buffer`
        /// to move the internal write position forward.
        ///
        /// Not calling `advance_buffer` with exactly the amount of written characters (which must
        /// also be contiguous and starting from the beginning of the buffer returned by this
        /// method) will cause unpredictable behavior.
        ///
        /// Calling `write` invalidates any ref previously returned by this method.
        ///
        /// Do not assume any particular alignment of the returned buffer or that all buffers
        /// returned from here are part of one continuous memory location. Subclasses are allowed
        /// to manage multiple seperate unaligned buffers of unspecific size internally.
        array_ref<void> get_buffer(Mode mode)
        {
            assert(manages_buffer());
            if( buffer_available > 0 )
                return current_buffer();
            return get_write_buffer(mode);
        }

        /// \brief Move the internal buffer position forward by \p count characters.
        void advance_buffer(size_t count) { advance(count); }

        /// \brief Move the internal buffer position to the buffer's end.
        void advance_whole_buffer() { advance(buffer_available); }

        /// \brief Directly write to the stream.
        ///
        /// This method may only be called if `manages_buffer` returns false.
        ///
        /// For streams that don't have internal buffers, this method can be used to add
        /// more efficient custom external buffer handling.
        ///
        /// The method is allowed to modify the elements of the \p buf_array to avoid the
        /// need to make a copy of the array.
        ///
        /// The combined sum of write requests (i.e. `io_vector_array_size_sum(buf_array)`)
        /// may not exceed `std::numeric_limits<ssize_t>::max()`.
        ///
        /// \p buf_array may be empty or have a combined sum of 0 without causing any errors.
        ssize_t direct_write(array_ref<io::io_vector> buf_array, Mode mode)
        {
            assert( ! manages_buffer());
            return write_stream(buf_array, mode);
        }

        bool has_error() const { return (status_flags & StatusError); }

        abstract_output_stream(const abstract_output_stream&) = delete;
        abstract_output_stream& operator=(const abstract_output_stream&) = delete;

    protected:
        abstract_output_stream(InternalBuffer internal_buffer) : abstract_stream_base(internal_buffer) {}

        abstract_output_stream(abstract_output_stream&&) = default;
        abstract_output_stream& operator=(abstract_output_stream&&) = default;

        virtual ssize_t write_stream(array_ref<io::io_vector> buf_array, Mode mode) = 0;
        virtual array_ref<void> LIBLBU_EXPORT get_write_buffer(Mode mode);
        virtual bool LIBLBU_EXPORT write_buffer_flush(Mode mode);
    };


    // some convenience helper

    static constexpr uint32_t DefaultBufferSize = (1 << 14); // Experimentally chosen, good memcpy/syscall ratio

    class incremental_reader {
    public:
        incremental_reader() {}
        incremental_reader(array_ref<void> dst) { reset(dst); }

        template< typename T >
        explicit incremental_reader(T* dst) { reset(dst); }

        void reset(array_ref<void> dst)
        {
            position = reinterpret_cast<char*>(dst.data());
            left = dst.byte_size();
        }

        template< typename T >
        void reset(T* dst)
        {
            static_assert(std::is_trivial_v<T>);
            reset(array_ref_one_element(dst));
        }

        bool read(abstract_input_stream* stream)
        {
            const auto c = stream->read(position, left, Mode::NonBlocking);
            if( c < 0 )
                return false;
            left -= size_t(c);
            if( left == 0 )
                return true;
            position += c;
            return false;
        }

    private:
        char* position = {};
        size_t left = 0;
    };


    class incremental_writer {
    public:
        incremental_writer() {}
        incremental_writer(array_ref<const void> src) { reset(src); }

        template< typename T >
        explicit incremental_writer(const T* src) { reset(src); }

        void reset(array_ref<const void> src)
        {
            position = reinterpret_cast<const char*>(src.data());
            left = src.byte_size();
        }

        template< typename T >
        void reset(const T* src)
        {
            static_assert(std::is_trivial_v<T>);
            reset(array_ref_one_element(src));
        }

        bool write(abstract_output_stream* stream)
        {
            const auto c = stream->write(position, left, Mode::NonBlocking);
            if( c < 0 )
                return false;
            left -= size_t(c);
            if( left == 0 )
                return true;
            position += c;
            return false;
        }

    private:
        const char* position = {};
        size_t left = 0;
    };

}
}

#endif
