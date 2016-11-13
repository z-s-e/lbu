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

#ifndef LIBLBU_ALIGN_H
#define LIBLBU_ALIGN_H

#include <stddef.h>
#include <stdint.h>

namespace lbu {

    template< typename T >
    inline uintptr_t alignment_mask()
    {
        return uintptr_t(alignof(T) - 1);
    }

    template< typename T >
    inline bool is_aligned(const void* ptr)
    {
        return (uintptr_t(ptr) & alignment_mask<T>()) == 0;
    }

    inline void* align_down(void* ptr, size_t alignment)
    {
        return reinterpret_cast<void*>(uintptr_t(ptr) & ~(alignment - 1));
    }

    inline const void* align_down(const void* ptr, size_t alignment)
    {
        return reinterpret_cast<const void*>(uintptr_t(ptr) & ~(alignment - 1));
    }

    inline size_t align_down_diff(uintptr_t ptr, size_t alignment)
    {
        return (ptr & (alignment - 1));
    }

    inline void* align_up(void* ptr, size_t alignment)
    {
        return reinterpret_cast<void*>((uintptr_t(ptr) + alignment - 1 ) & ~(alignment - 1));
    }

    inline const void* align_up(const void* ptr, size_t alignment)
    {
        return reinterpret_cast<const void*>((uintptr_t(ptr) + alignment - 1 ) & ~(alignment - 1));
    }

    inline size_t align_up_diff(uintptr_t ptr, size_t alignment)
    {
        auto tmp = alignment - (ptr & (alignment - 1));
        return tmp == alignment ? 0 : tmp;
    }

} // namespace lbu

#endif // LIBLBU_ALIGN_H
