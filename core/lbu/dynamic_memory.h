/* Copyright 2020 Zeno Sebastian Endemann <zeno.endemann@googlemail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LIBLBU_DYNAMIC_MEMORY_H
#define LIBLBU_DYNAMIC_MEMORY_H

#include "lbu/array_ref.h"
#include "lbu/unexpected.h"

#include <memory>
#include <stdlib.h>

namespace lbu {

    // memory allocation

    // TODO: use owner<T> instead of T*?

    template< typename T >
    T* malloc(size_t count)
    {
        static_assert(std::is_pod<T>::value, "only POD supported");
        if( count == 0 )
            return {};
        assert(count <= (std::numeric_limits<size_t>::max() / sizeof(T)));
        return static_cast<T*>(::malloc(sizeof(T) * count));
    }

    template< typename T >
    T* xmalloc(size_t count)
    {
        T* p = malloc<T>(count);
        if( count > 0 && p == nullptr )
            unexpected_memory_exhaustion();
        return p;
    }

    template< typename T >
    T* calloc(size_t count)
    {
        static_assert(std::is_pod<T>::value, "only POD supported");
        if( count == 0 )
            return {};
        assert(count <= (std::numeric_limits<size_t>::max() / sizeof(T)));
        return static_cast<T*>(::calloc(count, sizeof(T)));
    }

    template< typename T >
    T* xcalloc(size_t count)
    {
        T* p = calloc<T>(count);
        if( count > 0 && p == nullptr )
            unexpected_memory_exhaustion();
        return p;
    }

    template< typename T >
    T* realloc(T* p, size_t count)
    {
        static_assert(std::is_pod<T>::value, "only POD supported");
        assert(count <= (std::numeric_limits<size_t>::max() / sizeof(T)));
        return static_cast<T*>(::realloc(p, sizeof(T) * count));
    }

    template< typename T >
    T* xrealloc(T* p, size_t count)
    {
        p = realloc<T>(p, count);
        if( count > 0 && p == nullptr )
            unexpected_memory_exhaustion();
        return p;
    }

    template< typename T >
    T* malloc_aligned(size_t alignment, size_t count)
    {
        static_assert(std::is_pod<T>::value, "only POD supported");
        assert(is_pow2(alignment) && alignment >= alignof(T));
        if( count == 0 )
            return {};
        if( alignment <= alignof(max_align_t) )
            return malloc<T>(count);
        assert(count <= (std::numeric_limits<size_t>::max() / sizeof(T)));
        void* p = nullptr;
        if( ::posix_memalign(&p, alignment, sizeof(T) * count) != 0 )
            return nullptr;
        return static_cast<T*>(p);
    }

    template< typename T >
    T* xmalloc_aligned(size_t alignment, size_t count)
    {
        T* p = malloc_aligned<T>(alignment, count);
        if( count > 0 && p == nullptr )
            unexpected_memory_exhaustion();
        return p;
    }

    template< typename T >
    T* calloc_aligned(size_t alignment, size_t count)
    {
        static_assert(std::is_pod<T>::value, "only POD supported");
        assert(is_pow2(alignment) && alignment >= alignof(T));
        if( count == 0 )
            return {};
        if( alignment <= alignof(max_align_t) )
            return calloc<T>(count);
        assert(count <= (std::numeric_limits<size_t>::max() / sizeof(T)));
        void* p = nullptr;
        if( ::posix_memalign(&p, alignment, sizeof(T) * count) != 0 )
            return nullptr;
        std::memset(p, 0, sizeof(T) * count);
        return static_cast<T*>(p);
    }

    template< typename T >
    T* xcalloc_aligned(size_t alignment, size_t count)
    {
        T* p = calloc_aligned<T>(alignment, count);
        if( count > 0 && p == nullptr )
            unexpected_memory_exhaustion();
        return p;
    }

    template< typename T >
    T* malloc_copy(array_ref<const T> data)
    {
        auto p = malloc<T>(data.size());
        if( p != nullptr )
            std::memcpy(p, data.data(), data.byte_size());
        return p;
    }

    template< typename T >
    T* xmalloc_copy(array_ref<const T> data)
    {
        auto p = malloc_copy<T>(data);
        if( data.size() > 0 && p == nullptr )
            unexpected_memory_exhaustion();
        return p;
    }

    inline void* malloc_buffer(buffer_spec spec)
    {
        return malloc_aligned<char>(spec.align, spec.size);
    }

    inline void* xmalloc_buffer(buffer_spec spec)
    {
        return xmalloc_aligned<char>(spec.align, spec.size);
    }

    inline void* calloc_buffer(buffer_spec spec)
    {
        return calloc_aligned<char>(spec.align, spec.size);
    }

    inline void* xcalloc_buffer(buffer_spec spec)
    {
        return xcalloc_aligned<char>(spec.align, spec.size);
    }


    // c memory owning types

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

#endif // LIBLBU_DYNAMIC_MEMORY_H
