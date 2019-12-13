/* Copyright 2015-2019 Zeno Sebastian Endemann <zeno.endemann@googlemail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LIBLBU_MEMORY_H
#define LIBLBU_MEMORY_H

#include <cstdlib>
#include <cstring>
#include <memory>
#include <type_traits>

namespace lbu {

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
    T pod_from_raw_bytes(const void* src)
    {
        static_assert(std::is_pod<T>::value, "only POD supported");

        T result;
        std::memcpy(&result, src, sizeof(result));
        return result;
    }


    template< typename T, void(*CleanupFunction)(T) >
    struct static_cleanup_function {
        void operator()(T target) const { (*CleanupFunction)(target); }
    };

    template< typename T, void(*CleanupFunction)(T*) >
    using unique_ptr_c = std::unique_ptr<T, static_cleanup_function<T*, CleanupFunction> >;

    using unique_ptr_raw = unique_ptr_c<void, ::free>;

    template< typename T >
    using unique_cpod = std::unique_ptr<T, static_cleanup_function<void*, ::free> >;

    using unique_cstr = unique_cpod<char>;

} // namespace lbu

#endif // LIBLBU_MEMORY_H

