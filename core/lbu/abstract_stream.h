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

#ifndef LIBLBU_ABSTRACT_STREAM_H
#define LIBLBU_ABSTRACT_STREAM_H

#include "lbu/array_ref.h"
#include "lbu/lbu_global.h"
#include "lbu/io.h"

#include <algorithm>
#include <cstring>
#include <type_traits>
#include <unistd.h>

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
        abstract_stream_base(InternalBuffer bufferManagement)
            : managesBuffer(bufferManagement == InternalBuffer::Yes) {}

        bool buffer_management() const { return managesBuffer; }

        array_ref<char> current_buffer()
        {
            return {bufferBase + bufferOffset, bufferAvailable};
        }

        void advance(size_t count)
        {
            assert(count <= bufferAvailable);
            bufferOffset += count;
            bufferAvailable -= count;
        }

        char* bufferBase = {};
        uint32_t bufferOffset = 0;
        uint32_t bufferAvailable = 0;

        enum StatusFlags {
            StatusError = 1<<0,             ///< read error
            StatusEndOfStream = 1<<1        ///< at end
        };
        unsigned char statusFlags = 0;

    private:
        bool managesBuffer;
    };

} // namespace detail


    class abstract_input_stream : protected detail::abstract_stream_base {
    public:
        virtual LIBLBU_EXPORT ~abstract_input_stream();

        /// Read from the stream into buf.
        ///
        /// If mode is Blocking, the function returns size on success, a nonnegative value less
        /// than size when the stream ends before size characters are read, or a negative value
        /// on a stream error.
        ///
        /// If mode is NonBlocking, the function may read less than size characters without
        /// error or end of stream (possibly 0). A negative return value signals a stream error.
        ssize_t read(void* buf, size_t size, Mode mode)
        {
            if( bufferAvailable >= size && bufferAvailable > 0 ) {
                assert(manages_buffer());
                assert(bufferBase != nullptr);
                std::memcpy(buf, bufferBase + bufferOffset, size);
                advance_buffer(size);
                return ssize_t(size);
            }
            auto b = io::io_vec(static_cast<char*>(buf), size);
            return read_stream(array_ref_one_element(&b),
                               mode == Mode::Blocking ? size : 0);
        }

        /// True iff the stream manages internal buffer.
        ///
        /// Note that the internal buffer can already be the stream destination (e.g. a mmapped
        /// region of a file is also called an internal buffer here).
        ///
        /// Subclasses set this at construction time and cannot change it for the object's
        /// lifetime.
        bool manages_buffer() const { return buffer_management(); }

        /// Return a ref to the internal buffer for zero copy access.
        ///
        /// Only call this after checking availabilty with manages_buffer().
        ///
        /// If mode is Blocking, an empty (invalid) array_ref indicates either a stream error or
        /// end of stream, otherwise a valid ref is returned. In NonBlocking mode an invalid ref may
        /// also be returned when no data is available without blocking.
        ///
        /// If a valid ref is returned, it will point to a buffer containing the next data in the
        /// stream. Once done with reading, one must call advance_buffer(..) resp.
        /// advance_whole_buffer() to move the internal read position forward.
        ///
        /// Calling read() invalidates any ref previously returned by this method.
        ///
        /// Do not assume any particular alignment of the returned buffer or that all buffers
        /// returned from here are part of one continuous memory location. Subclasses are allowed
        /// to manage multiple seperate unaligned buffers of unspecific size internally.
        ///
        /// Writing into the internal buffer is not allowed.
        array_ref<const void> get_buffer(Mode mode)
        {
            assert(manages_buffer());
            if( bufferAvailable > 0 )
                return current_buffer();
            return get_read_buffer(mode);
        }

        /// Move the internal buffer position forward by count characters.
        void advance_buffer(size_t count) { advance(count); }

        /// Move the internal buffer position to the buffer's end.
        void advance_whole_buffer() { advance(bufferAvailable); }

        /// Directly read from the stream.
        ///
        /// This method may only be called if manages_buffer() is false.
        ///
        /// For streams that don't have internal buffers this method can be used to add
        /// more efficient custom external buffer handling.
        ///
        /// When required_read is greater than 0 the method is allowed to modify the
        /// io_vector elements to avoid the need to make a copy of the array.
        ///
        /// On success the return value is the total amount of characters read (which
        /// is greater or equal to required_read). Otherwise a stream error occured and a
        /// negative value is returned.
        ssize_t direct_read(array_ref<io::io_vector> buf_array, size_t required_read)
        {
            assert(!manages_buffer());
            return read_stream(buf_array, required_read);
        }

        bool has_error() const { return (statusFlags & StatusError); }
        bool at_end() const { return (statusFlags & StatusEndOfStream); }

        abstract_input_stream(const abstract_input_stream&) = delete;
        abstract_input_stream& operator=(const abstract_input_stream&) = delete;

    protected:
        abstract_input_stream(InternalBuffer bufferManagement) : abstract_stream_base(bufferManagement) {}

        abstract_input_stream(abstract_input_stream&&) = default;
        abstract_input_stream& operator=(abstract_input_stream&&) = default;

        virtual ssize_t read_stream(array_ref<io::io_vector> buf_array, size_t required_read) = 0;
        virtual LIBLBU_EXPORT array_ref<const void> get_read_buffer(Mode mode);
    };


    class abstract_output_stream : protected detail::abstract_stream_base {
    public:
        /// Destructor.
        ///
        /// Note that this does not call flush_buffer, it is the user's responsibility
        /// to properly flush all buffers before the destructor is run.
        ///
        /// Rationale: flushing is an operation that can fail or block, it should not be
        /// called implicitly.
        virtual LIBLBU_EXPORT ~abstract_output_stream();

        /// Write from buf to the stream.
        ///
        /// If mode is Blocking, the function returns size on success or a negative value
        /// on a stream error.
        ///
        /// If mode is NonBlocking, the function may write less than size characters without
        /// error (possibly 0). A negative return value signals a stream error.
        ssize_t write(const void* buf, size_t size, Mode mode)
        {
            if( bufferAvailable >= size && bufferAvailable > 0 ) {
                assert(manages_buffer());
                assert(bufferBase != nullptr);
                std::memcpy(bufferBase + bufferOffset, buf, size);
                advance_buffer(size);
                return ssize_t(size);
            }
            auto b = io::io_vec(const_cast<void*>(buf), size);
            return write_stream(array_ref_one_element(&b), mode);
        }

        /// True iff the stream manages internal buffer.
        ///
        /// Note that the internal buffer can already be the stream destination (e.g. a mmapped
        /// region of a file is also called an internal buffer here).
        ///
        /// Subclasses set this at construction time and cannot change it for the object's
        /// lifetime.
        bool manages_buffer() const { return buffer_management(); }

        /// Flush internal buffer.
        ///
        /// Returns true when all internal buffer have been flushed. If mode is Blocking, returning
        /// false indicates a stream error. In NonBlocking mode, the method may return false even when
        /// no error occured (but flushing the internal buffers would block).
        bool flush_buffer(Mode mode = Mode::Blocking)
        {
            if( ! manages_buffer() )
                return true;
            return write_buffer_flush(mode);
        }

        /// Return a ref to the internal buffer for zero copy access.
        ///
        /// Only call this after checking availabilty with manages_buffer().
        ///
        /// If mode is Blocking, an empty (invalid) array_ref indicates a stream error, otherwise a
        /// valid ref is returned. In NonBlocking mode an invalid ref may be returned even if no
        /// error occured.
        ///
        /// If a valid ref is returned, it will point to a buffer one can write into.
        /// Once done with writing, one must call advance_buffer(..) resp. advance_whole_buffer()
        /// to move the internal write position forward.
        ///
        /// Not calling advance_buffer with exactly the amount of written characters (which must
        /// also be contiguous and starting from the beginning of the buffer returned by this
        /// method) will cause unpredictable behavior.
        ///
        /// Calling write() invalidates any ref previously returned by this method.
        ///
        /// Do not assume any particular alignment of the returned buffer or that all buffers
        /// returned from here are part of one continuous memory location. Subclasses are allowed
        /// to manage multiple seperate unaligned buffers of unspecific size internally.
        array_ref<void> get_buffer(Mode mode)
        {
            assert(manages_buffer());
            if( bufferAvailable > 0 )
                return current_buffer();
            return get_write_buffer(mode);
        }

        /// Move the internal buffer position forward by count characters.
        void advance_buffer(size_t count) { advance(count); }

        /// Move the internal buffer position to the buffer's end.
        void advance_whole_buffer() { advance(bufferAvailable); }

        /// Directly write to the stream.
        ///
        /// This method may only be called if manages_buffer() is false.
        ///
        /// For streams that don't have internal buffers this method can be used to add
        /// more efficient custom external buffer handling.
        ///
        /// The method is allowed to modify the io_vector elements in blocking mode,
        /// to avoid the need to make a copy of the array.
        ssize_t direct_write(array_ref<io::io_vector> buf_array, Mode mode)
        {
            assert(!manages_buffer());
            return write_stream(buf_array, mode);
        }

        bool has_error() const { return (statusFlags & StatusError); }

        abstract_output_stream(const abstract_output_stream&) = delete;
        abstract_output_stream& operator=(const abstract_output_stream&) = delete;

    protected:
        abstract_output_stream(InternalBuffer bufferManagement) : abstract_stream_base(bufferManagement) {}

        abstract_output_stream(abstract_output_stream&&) = default;
        abstract_output_stream& operator=(abstract_output_stream&&) = default;

        virtual ssize_t write_stream(array_ref<io::io_vector> buf_array, Mode mode) = 0;
        virtual LIBLBU_EXPORT array_ref<void> get_write_buffer(Mode mode);
        virtual LIBLBU_EXPORT bool write_buffer_flush(Mode mode);
    };


    // some convenience helper

    static constexpr uint32_t DefaultBufferSize = (1 << 14); // Experimentally chosen, good memcpy/syscall ratio

    class incremental_reader {
    public:
        template<typename T>
        explicit incremental_reader(T* dst) { reset(dst); }

        void reset(void *dst, size_t size)
        {
            position = reinterpret_cast<char*>(dst);
            left = size;
        }

        template<typename T>
        void reset(T* dst)
        {
            static_assert(std::is_pod<T>::value, "Type must be POD");
            reset(dst, sizeof(T));
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
        char* position;
        size_t left;
    };


    class incremental_writer {
    public:
        template<typename T>
        explicit incremental_writer(const T* dst) { reset(dst); }

        void reset(const void *dst, size_t size)
        {
            position = reinterpret_cast<const char*>(dst);
            left = size;
        }

        template<typename T>
        void reset(const T* dst)
        {
            static_assert(std::is_pod<T>::value, "Type must be POD");
            reset(dst, sizeof(T));
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
        const char* position;
        size_t left;
    };

} // namespace stream
} // namespace lbu

#endif // LIBLBU_ABSTRACT_STREAM_H
