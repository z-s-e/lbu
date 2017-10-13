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

#ifndef LIBLBU_ALIGN_H
#define LIBLBU_ALIGN_H

#include "lbu/math.h"

#include <cassert>
#include <stddef.h>
#include <stdint.h>

namespace lbu {

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
        return align_down(offset + alignment - 1, alignment);
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

} // namespace lbu

#endif // LIBLBU_ALIGN_H
