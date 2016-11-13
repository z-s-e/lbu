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

#ifndef LIBLBU_MEMORY_H
#define LIBLBU_MEMORY_H

#include <cstring>
#include <memory>
#include <type_traits>

namespace lbu {

    template< typename T, typename U >
    U value_reinterpret_cast(T value)
    {
        static_assert(sizeof(T) == sizeof(U), "size mismatch");
        static_assert(std::is_pod<T>::value && std::is_pod<U>::value, "only POD supported");

        U result = {};
        std::memcpy(&result, &value, sizeof(result));
        return result;
    }


    template< typename T >
    T pod_from_raw_bytes(const void* src)
    {
        static_assert(std::is_pod<T>::value, "only POD supported");

        T v = {};
        std::memcpy(&v, src, sizeof(v));
        return v;
    }


    template< typename T, void(*CleanupFunction)(T) >
    struct static_cleanup_function {
        void operator()(T target) const { (*CleanupFunction)(target); }
    };

    template< typename T, void(*CleanupFunction)(T*) >
    using unique_ptr_c = std::unique_ptr< T, static_cleanup_function<T*, CleanupFunction > >;

    using unique_ptr_raw = unique_ptr_c<void, ::free>;

    using unique_cstr = std::unique_ptr< char, static_cleanup_function<void*, ::free > >;

} // namespace lbu

#endif // LIBLBU_MEMORY_H

