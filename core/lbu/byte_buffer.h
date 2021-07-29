/* Copyright 2015-2020 Zeno Sebastian Endemann <zeno.endemann@mailbox.org>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LIBLBU_BYTE_BUFFER_H
#define LIBLBU_BYTE_BUFFER_H

#include "lbu/array_ref.h"
#include "lbu/dynamic_memory.h"

#include <algorithm>
#include <endian.h>
#include <stdint.h>

#ifndef __BYTE_ORDER
#error Need __BYTE_ORDER
#elif __BYTE_ORDER == __LITTLE_ENDIAN
//ok
#elif __BYTE_ORDER == __BIG_ENDIAN
#error Untested byte order
#else
#error Unsupported byte order
#endif

namespace lbu {

    // Similar to std::string (including small string optimization), but:
    // - does not add a 0 byte terminator at the end automatically
    // - uses realloc for appending
    // - more compact on 64 bit, but only allows ~2GB max size
    // - can adopt/release malloced buffer
    // - allows uninitialized insert/append
    class byte_buffer {
    public:
        byte_buffer() = default;
        ~byte_buffer();
        explicit byte_buffer(array_ref<const void> data);

        byte_buffer(const byte_buffer& other);
        byte_buffer& operator=(const byte_buffer& other);
        byte_buffer(byte_buffer&& other);
        byte_buffer& operator=(byte_buffer&& other);

        size_t size() const;
        bool is_empty() const;
        size_t capacity() const;

        void* data();
        const void* data() const;

        array_ref<void> ref();
        array_ref<const void> ref() const;

        void reserve(size_t capacity);
        void shrink_to_fit();
        void set_capacity(size_t capacity);
        void auto_grow_reserve();

        void clear();
        void resize(size_t count);
        void chop(size_t count);

        byte_buffer& insert(size_t index, size_t count, unsigned char ch = 0);
        byte_buffer& insert(size_t index, size_t count, signed char ch);
        byte_buffer& insert(size_t index, size_t count, char ch);
        byte_buffer& insert(size_t index, array_ref<const void> data);
        array_ref<void> insert_uninitialized(size_t index, size_t count);

        byte_buffer& append(size_t count, unsigned char ch = 0);
        byte_buffer& append(size_t count, signed char ch);
        byte_buffer& append(size_t count, char ch);
        byte_buffer& append(array_ref<const void> data);
        array_ref<void> append_begin();
        void append_commit(size_t count);

        byte_buffer& erase(size_t index, size_t count);

        void adopt_raw_malloc(unique_ptr_raw data, size_t size);
        void adopt_raw_malloc(unique_ptr_raw data, size_t size, size_t capacity);
        unique_ptr_raw release_raw_malloc();

        static constexpr size_t max_size() { return (size_t(1) << 31) - 1; }

    private:
        bool is_small() const { return d.small.size & SmallFlag; }
        size_t small_size() const;
        void set_small_size(size_t size);
        void set_small(const void* data, size_t size);

        char* char_data() const;
        array_ref<char> char_ref() const;

        size_t ext_capacity() const;
        void set_ext_capacity(size_t capacity);
        void set_ext(char* data, size_t size, size_t capacity);

        void set_size_checked(size_t size);
        void set_capacity_checked(size_t capacity);

        array_ref<void> append_base(size_t count);

        void cleanup();
        void move_from(byte_buffer& other);
        size_t grow_capacity(size_t size) const;

        // data

        struct external_data {
            uint32_t reserved;
            uint32_t size;
            char* data;
        };

        static constexpr unsigned SmallResered = sizeof(external_data) - 1;
#if __BYTE_ORDER == __LITTLE_ENDIAN
        static constexpr uint8_t SmallFlag = 1;
#else
        static constexpr uint8_t SmallFlag = 1 << 7;
#endif

        struct small_data {
            uint8_t size = SmallFlag;
            char data[SmallResered];
        };
        static_assert(sizeof(small_data) == sizeof(external_data), "bug");

        union data {
            data() : small() {}

            small_data small;
            external_data ext;
        } d;
    };

    // implementation

    inline byte_buffer::~byte_buffer() { cleanup(); }

    inline byte_buffer::byte_buffer(array_ref<const void> data)
    {
        const auto s = data.byte_size();
        if( s <= SmallResered ) {
            set_small(data.data(), s);
        } else {
            set_ext(xmalloc_copy(data.array_static_cast<char>()), s, s);
        }
    }

    inline byte_buffer::byte_buffer(const byte_buffer& other)
        : byte_buffer(other.ref())
    {
    }

    inline byte_buffer& byte_buffer::operator=(const byte_buffer& other)
    {
        return operator =(byte_buffer(other));
    }

    inline byte_buffer::byte_buffer(byte_buffer&& other)
    {
        move_from(other);
    }

    inline byte_buffer& byte_buffer::operator=(byte_buffer&& other)
    {
        cleanup();
        move_from(other);
        return *this;
    }

    inline size_t byte_buffer::size() const
    {
        return is_small() ? small_size() : d.ext.size;
    }

    inline bool byte_buffer::is_empty() const { return size() == 0; }

    inline size_t byte_buffer::capacity() const
    {
        return is_small() ? SmallResered : ext_capacity();
    }

    inline array_ref<void> byte_buffer::ref() { return char_ref(); }

    inline array_ref<const void> byte_buffer::ref() const { return char_ref(); }

    inline const void* byte_buffer::data() const { return char_data(); }

    inline void* byte_buffer::data() { return char_data(); }

    inline void byte_buffer::reserve(size_t capacity)
    {
        if( capacity <= this->capacity() )
            return;
        set_capacity_checked(capacity);
    }

    inline void byte_buffer::shrink_to_fit() { set_capacity_checked(size()); }

    inline void byte_buffer::set_capacity(size_t capacity)
    {
        set_capacity_checked(std::max(capacity, size()));
    }

    inline void byte_buffer::auto_grow_reserve()
    {
        const auto cap = capacity();
        if( cap < max_size() )
            set_capacity_checked(grow_capacity(cap + 1));
    }

    inline void byte_buffer::clear() { set_size_checked(0); }

    inline void byte_buffer::resize(size_t count)
    {
        const auto s = size();
        if( count > s )
            append(count - s);
        else
            set_size_checked(count);
    }

    inline void byte_buffer::chop(size_t count)
    {
        const auto s = size();
        set_size_checked(count > s ? 0 : s - count);
    }

    inline byte_buffer& byte_buffer::insert(size_t index, size_t count, unsigned char ch)
    {
        auto a = insert_uninitialized(index, count);
        std::memset(a.data(), ch, count);
        return *this;
    }

    inline byte_buffer& byte_buffer::insert(size_t index, size_t count, signed char ch)
    {
        return insert(index, count, reinterpret_cast<unsigned char&>(ch));
    }

    inline byte_buffer& byte_buffer::insert(size_t index, size_t count, char ch)
    {
        return insert(index, count, reinterpret_cast<unsigned char&>(ch));
    }

    inline byte_buffer &byte_buffer::insert(size_t index, array_ref<const void> data)
    {
        auto a = insert_uninitialized(index, data.byte_size());
        std::memcpy(a.data(), data.data(), a.byte_size());
        return *this;
    }

    inline array_ref<void> byte_buffer::insert_uninitialized(size_t index, size_t count)
    {
        const auto oldSize = size();
        assert(index <= oldSize);
        assert(max_size() - oldSize >= count);
        const auto newSize = oldSize + count;
        char* c = char_data();

        if( capacity() - oldSize >= count ) {
            c += index;
            std::memmove(c + count, c, oldSize - index);
            set_size_checked(newSize);
        } else {
            const auto newCapacity = grow_capacity(newSize);
            char* newData = xmalloc_bytes<char>(newCapacity);
            char* newC = newData + index;
            std::memcpy(newData, c, index);
            std::memcpy(newC + count, c + index,  oldSize - index);
            cleanup();
            set_ext(newData, newSize, newCapacity);
            c = newC;
        }
        return array_ref<char>(c, count);
    }

    inline byte_buffer& byte_buffer::append(size_t count, unsigned char ch)
    {
        std::memset(append_base(count).data(), ch, count);
        return *this;
    }

    inline byte_buffer& byte_buffer::append(size_t count, signed char ch)
    {
        return append(count, reinterpret_cast<unsigned char&>(ch));
    }

    inline byte_buffer& byte_buffer::append(size_t count, char ch)
    {
        return append(count, reinterpret_cast<unsigned char&>(ch));
    }

    inline byte_buffer& byte_buffer::append(array_ref<const void> data)
    {
        auto a = append_base(data.byte_size());
        std::memcpy(a.data(), data.data(), data.byte_size());
        return *this;
    }

    inline array_ref<void> byte_buffer::append_begin()
    {
        const auto s = size();
        return array_ref<char>(char_data() + s, capacity() - s);
    }

    inline void byte_buffer::append_commit(size_t count)
    {
        const auto s = size();
        assert(capacity() - s >= count);
        set_size_checked(s + count);
    }

    inline byte_buffer& byte_buffer::erase(size_t index, size_t count)
    {
        const auto oldSize = size();
        assert(index <= oldSize);
        count = std::min(oldSize - index, count);
        const auto secondIndex = index + count;
        const auto newSize = oldSize - count;
        char* c = char_data();
        std::memmove(c + index, c + secondIndex, oldSize - secondIndex);
        d.ext.size = newSize;
        return *this;
    }

    inline void byte_buffer::adopt_raw_malloc(unique_ptr_raw data, size_t size)
    {
        adopt_raw_malloc(std::move(data), size, size);
    }

    inline void byte_buffer::adopt_raw_malloc(unique_ptr_raw data, size_t size, size_t capacity)
    {
        assert(capacity >= size);
        cleanup();
        if( capacity <= SmallResered ) {
            set_small(data.get(), size);
        } else {
            set_ext(static_cast<char*>(data.release()), size, capacity);
        }
    }

    inline unique_ptr_raw byte_buffer::release_raw_malloc()
    {
        if( is_small() )
            return {};
        char* c = d.ext.data;
        set_small_size(0);
        return unique_ptr_raw(c);
    }

    inline size_t byte_buffer::small_size() const
    {
#if __BYTE_ORDER == __LITTLE_ENDIAN
        return size_t(d.small.size) >> 1;
#else
        return (d.small.size & ~SmallFlag);
#endif
    }

    inline void byte_buffer::set_small_size(size_t size)
    {
        assert(size <= SmallResered);
#if __BYTE_ORDER == __LITTLE_ENDIAN
        d.small.size = SmallFlag | (size << 1);
#else
        d.small.size = SmallFlag | size;
#endif
    }

    inline void byte_buffer::set_small(const void* data, size_t size)
    {
        set_small_size(size);
        std::memcpy(d.small.data, data, size);
    }

    inline char* byte_buffer::char_data() const
    {
        return is_small() ? const_cast<char*>(d.small.data) : d.ext.data;
    }

    inline array_ref<char> byte_buffer::char_ref() const
    {
        return array_ref<char>(char_data(), size());
    }

    inline size_t byte_buffer::ext_capacity() const
    {
#if __BYTE_ORDER == __LITTLE_ENDIAN
        return (d.ext.reserved >> 1) + 1;
#else
        return d.ext.reserved + 1;
#endif
    }

    inline void byte_buffer::set_ext_capacity(size_t capacity)
    {
        assert(capacity > SmallResered && capacity <= max_size());
#if __BYTE_ORDER == __LITTLE_ENDIAN
        d.ext.reserved = (capacity - 1) << 1;
#else
        d.ext.reserved = capacity - 1;
#endif
    }

    inline void byte_buffer::set_ext(char *data, size_t size, size_t capacity)
    {
        d.ext.data = data;
        d.ext.size = size;
        set_ext_capacity(capacity);
    }

    inline void byte_buffer::set_size_checked(size_t size)
    {
        if( is_small() )
            set_small_size(size);
        else
            d.ext.size = size;
    }

    inline void byte_buffer::set_capacity_checked(size_t capacity)
    {
        auto r = ref();

        if( capacity <= SmallResered ) {
            if( is_small() )
                return;
            set_small(r.data(), r.size());
            ::free(r.data());
        } else if( is_small() ) {
            char* c = xmalloc_bytes<char>(capacity);
            std::memcpy(c, r.data(), r.size());
            // memcpy must happen first
            set_ext(c, r.size(), capacity);
        } else {
            set_ext(xrealloc_bytes<char>(d.ext.data, capacity), r.size(), capacity);
        }
    }

    inline array_ref<void> byte_buffer::append_base(size_t count)
    {
        const auto oldSize = size();
        assert(max_size() - oldSize >= count);
        const auto newSize = oldSize + count;
        char* c = char_data();

        if( capacity() < newSize ) {
            const auto newCapacity = grow_capacity(newSize);
            char* newC;

            if( is_small() ) {
                newC = xmalloc_bytes<char>(newCapacity);
                std::memcpy(newC, c, oldSize);
            } else {
                newC = xrealloc_bytes<char>(c, newCapacity);
            }
            set_ext(newC, newSize, newCapacity);
            c = newC;
        } else {
            set_size_checked(newSize);
        }

        return array_ref<char>(c + oldSize, count);
    }

    inline void byte_buffer::cleanup()
    {
        if( ! is_small() )
            ::free(d.ext.data);
    }

    inline void byte_buffer::move_from(byte_buffer& other)
    {
        d = other.d;
        if( ! is_small() )
            other.set_small_size(0);
    }

    inline size_t byte_buffer::grow_capacity(size_t size) const
    {
        assert(size <= max_size());
        const auto cap = capacity();
        if( cap == SmallResered )
            return std::max(size_t(32), size);
        if( cap >= max_size() / 2 )
            return max_size();
        return std::max(cap * 2, size);
    }

} // namespace lbu

#endif // LIBLBU_BYTE_BUFFER_H
