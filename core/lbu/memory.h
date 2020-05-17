/* Copyright 2015-2020 Zeno Sebastian Endemann <zeno.endemann@googlemail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LIBLBU_MEMORY_H
#define LIBLBU_MEMORY_H

#include "lbu/math.h"

#include <cstring>
#include <stddef.h>
#include <stdint.h>
#include <type_traits>
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
        assert(tmp >= sizeof(void*));
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


    // buffer spec

    struct buffer_spec {
        size_t size = 0;
        size_t align = 0;

        bool is_valid() const { return size > 0 && is_pow2(align); }
        explicit operator bool() const { return is_valid(); }
    };

} // namespace lbu

#endif // LIBLBU_MEMORY_H
