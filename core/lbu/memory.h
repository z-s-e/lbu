/* Copyright 2015-2020 Zeno Sebastian Endemann <zeno.endemann@mailbox.org>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LIBLBU_MEMORY_H
#define LIBLBU_MEMORY_H

#include "lbu/math.h"

#include <cstring>
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

namespace lbu {

    // memory alignment

    constexpr uintptr_t alignment_mask(size_t alignment)
    {
        assert(is_pow2(alignment));
        return uintptr_t(alignment - 1);
    }

    template< typename T >
    constexpr uintptr_t alignment_mask()
    {
        return alignment_mask(alignof(T));
    }

    constexpr bool is_aligned(uintptr_t offset, size_t alignment)
    {
        return (offset & alignment_mask(alignment)) == 0;
    }

    inline bool is_aligned(const void* ptr, size_t alignment)
    {
        return is_aligned(uintptr_t(ptr), alignment);
    }

    template< typename T >
    bool is_aligned(const void* ptr)
    {
        return is_aligned(ptr, alignof(T));
    }

    constexpr uintptr_t align_down(uintptr_t offset, size_t alignment)
    {
        return offset & ~alignment_mask(alignment);
    }

    inline void* align_down(void* ptr, size_t alignment)
    {
        return reinterpret_cast<void*>(align_down(uintptr_t(ptr), alignment));
    }

    inline const void* align_down(const void* ptr, size_t alignment)
    {
        return reinterpret_cast<const void*>(align_down(uintptr_t(ptr), alignment));
    }

    constexpr size_t align_down_diff(uintptr_t offset, size_t alignment)
    {
        return (offset & alignment_mask(alignment));
    }

    constexpr uintptr_t align_up(uintptr_t offset, size_t alignment)
    {
        assert(offset <= std::numeric_limits<uintptr_t>::max() - (alignment - 1));
        return align_down(offset + (alignment - 1), alignment);
    }

    inline void* align_up(void* ptr, size_t alignment)
    {
        return reinterpret_cast<void*>(align_up(uintptr_t(ptr), alignment));
    }

    inline const void* align_up(const void* ptr, size_t alignment)
    {
        return reinterpret_cast<const void*>(align_up(uintptr_t(ptr), alignment));
    }

    constexpr size_t align_up_diff(uintptr_t offset, size_t alignment)
    {
        auto tmp = alignment - align_down_diff(offset, alignment);
        return tmp == alignment ? 0 : tmp;
    }

    inline size_t memory_interference_alignment()
    {
        long tmp = sysconf(_SC_LEVEL1_DCACHE_LINESIZE);
        // I do not see a reason for this call to ever fail,
        // so for now just assert on a sane value.
        assert(tmp >= long(alignof(max_align_t)));
        auto align = size_t(tmp);
        assert(is_pow2(align));
        return align;
    }


    // memory reinterpretation

    template< typename T, typename U >
    U value_reinterpret_cast(const T& value)
    {
        static_assert(sizeof(T) == sizeof(U), "size mismatch");
        static_assert(std::is_pod<T>::value && std::is_pod<U>::value, "only POD supported");

        U result;
        std::memcpy(&result, &value, sizeof(result));
        return result;
    }

    template< typename T >
    T byte_reinterpret_cast(const void* src)
    {
        static_assert(std::is_pod<T>::value, "only POD supported");

        T result;
        std::memcpy(&result, src, sizeof(result));
        return result;
    }


    // dynamic data

    struct buffer_spec {
        buffer_spec() = default;
        buffer_spec(size_t byte_size, size_t alignment) : size(byte_size), align(alignment) {}

        size_t size = 0;
        size_t align = 1;

        bool is_valid() const
        {
            return size < std::numeric_limits<size_t>::max() && is_pow2(align);
        }
        explicit operator bool() const { return is_valid(); }
    };

    class dynamic_struct {
    public:
        template< typename T >
        struct member_offset {
            size_t offset;

            member_offset<void> raw() const { return {offset}; }
        };

        template< typename T >
        member_offset<T> add_member(size_t count = 1,
                                    size_t alignment = 0,
                                    size_t* align_padding = nullptr)
        {
            assert(count > 0 && count < std::numeric_limits<size_t>::max() / sizeof(T));
            buffer_spec s;
            s.size = count * sizeof(T);
            s.align = (alignment == 0 ? alignof(T) : alignment);
            assert(s.align >= alignof(T));
            return { add_member_raw(s, align_padding).offset };
        }

        member_offset<void> add_member_raw(buffer_spec s, size_t* align_padding = nullptr)
        {
            assert(s.is_valid());
            spec.align = std::max(spec.align, s.align);
            size_t off = spec.size;
            if( off > std::numeric_limits<size_t>::max() - (s.align - 1) )
                off = std::numeric_limits<size_t>::max();
            else
                off = align_up(off, s.align);
            if( align_padding != nullptr )
                *align_padding += (off - spec.size);
            if( off > std::numeric_limits<size_t>::max() - s.size )
                spec.size = std::numeric_limits<size_t>::max();
            else
                spec.size = off + s.size;
            return { off };
        }

        void add_align_padding(size_t alignment)
        {
            assert(is_pow2(alignment));
            if( spec.size > std::numeric_limits<size_t>::max() - (alignment - 1) )
                spec.size = std::numeric_limits<size_t>::max();
            else
                spec.size = align_up(spec.size, alignment);
        }

        bool is_valid() const { return spec.is_valid(); }
        explicit operator bool() const { return is_valid(); }
        buffer_spec storage() const { return spec; }

        template< typename T >
        void* resolve(void* ptr, member_offset<T> off) const
        {
            assert(is_valid());
            assert(off.offset < spec.size);
            auto p = static_cast<char*>(ptr) + off.offset;
            return p;
        }

    private:
        buffer_spec spec;
    };


    // byte types

    template< typename T > struct is_byte_type : std::false_type {};
    template<> struct is_byte_type<char> : std::true_type {};
    template<> struct is_byte_type<signed char> : std::true_type {};
    template<> struct is_byte_type<unsigned char> : std::true_type {};
    template<> struct is_byte_type<std::byte> : std::true_type {};

} // namespace lbu

#endif // LIBLBU_MEMORY_H
