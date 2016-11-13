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

#ifndef LIBLBU_XMALLOC_H
#define LIBLBU_XMALLOC_H

#include "lbu/array_ref.h"
#include "lbu/lbu_global.h"

#include <cassert>
#include <cerrno>
#include <cstring>
#include <stdlib.h>

namespace lbu {

    template< typename T >
    array_ref<T> xmalloc(size_t count)
    {
        static_assert(std::is_pod<T>::value, "only POD supported");
        if( count == 0 )
            return {};
        assert(count <= (std::numeric_limits<size_t>::max() / (2 * sizeof(T))));
        void* p = ::malloc(sizeof(T) * count);
        if( p == nullptr )
            throw std::bad_alloc();
        return {static_cast<T*>(p), count};
    }

    template< typename T >
    array_ref<T> xcalloc(size_t count)
    {
        static_assert(std::is_pod<T>::value, "only POD supported");
        if( count == 0 )
            return {};
        assert(count <= (std::numeric_limits<size_t>::max() / (2 * sizeof(T))));
        void* p = ::calloc(count, sizeof(T));
        if( p == nullptr )
            throw std::bad_alloc();
        return {static_cast<T*>(p), count};
    }

    template< typename T >
    array_ref<T> xrealloc(void* p, size_t count)
    {
        static_assert(std::is_pod<T>::value, "only POD supported");
        assert(count <= (std::numeric_limits<size_t>::max() / (2 * sizeof(T))));
        p = ::realloc(p, sizeof(T) * count);
        if( count == 0 )
            return {};
        if( p == nullptr )
            throw std::bad_alloc();
        return {static_cast<T*>(p), count};
    }

    template< typename T >
    array_ref<T> xmalloc_aligned(size_t alignment, size_t count)
    {
        static_assert(std::is_pod<T>::value, "only POD supported");
        assert(alignment >= sizeof(void*));
        if( count == 0 )
            return {};
        assert(count <= (std::numeric_limits<size_t>::max() / (2 * sizeof(T))));
        void* p = nullptr;
        int ret = ::posix_memalign(&p, alignment, sizeof(T) * count);
        if( ret != 0 ) {
            if( ret == EINVAL )
                assert(false); // alignment was not a power of two
            throw std::bad_alloc();
        }
        return {static_cast<T*>(p), count};
    }

    template< typename T >
    array_ref<T> xmalloc_copy(array_ref<const T> data)
    {
        auto a = xmalloc<T>(data.size());
        std::memcpy(a.data(), data.data(), data.byte_size());
        return a;
    }

} // namespace lbu

#endif // LIBLBU_XMALLOC_H
