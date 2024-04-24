/* Copyright 2020 Zeno Sebastian Endemann <zeno.endemann@mailbox.org>
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

    template< typename ByteType >
    ByteType* malloc_bytes(size_t count)
    {
        static_assert(is_byte_type<ByteType>::value);
        if( count == 0 )
            return {};
        return static_cast<ByteType*>(::malloc(count));
    }

    template< typename ByteType >
    ByteType* xmalloc_bytes(size_t count)
    {
        ByteType* p = malloc_bytes<ByteType>(count);
        if( count > 0 && p == nullptr )
            unexpected_memory_exhaustion();
        return p;
    }

    template< typename ByteType >
    ByteType* realloc_bytes(ByteType* p, size_t count)
    {
        static_assert(is_byte_type<ByteType>::value);
        return static_cast<ByteType*>(::realloc(p, count));
    }

    template< typename ByteType >
    ByteType* xrealloc_bytes(ByteType* p, size_t count)
    {
        p = realloc_bytes<ByteType>(p, count);
        if( count > 0 && p == nullptr )
            unexpected_memory_exhaustion();
        return p;
    }

    inline void* malloc(buffer_spec spec)
    {
        if( spec.size == 0 )
            return {};
        assert(spec.is_valid());
        if( spec.align <= alignof(max_align_t) )
            return malloc_bytes<char>(spec.size);
        void* p = {};
        if( ::posix_memalign(&p, spec.align, spec.size) != 0 )
            return {};
        return p;
    }

    inline void* xmalloc(buffer_spec spec)
    {
        void* p = malloc(spec);
        if( spec.size > 0 && p == nullptr )
            unexpected_memory_exhaustion();
        return p;
    }

    template< typename T >
    T* malloc_copy(array_ref<const T> data)
    {
        static_assert(std::is_trivial_v<T>);
        auto p = malloc_bytes<char>(data.byte_size());
        if( p != nullptr )
            std::memcpy(p, data.data(), data.byte_size());
        return p;
    }

    template< typename Pod >
    Pod* xmalloc_copy(array_ref<const Pod> data)
    {
        auto p = malloc_copy<Pod>(data);
        if( data.size() > 0 && p == nullptr )
            unexpected_memory_exhaustion();
        return p;
    }

    template< typename T >
    T* calloc(size_t count)
    {
        static_assert(std::is_trivial_v<T>);
        if( count == 0 )
            return {};
        assert(count <= (std::numeric_limits<size_t>::max() / sizeof(T)));
        return static_cast<T*>(::calloc(count, sizeof(T)));
    }

    template< typename Pod >
    Pod* xcalloc(size_t count)
    {
        Pod* p = calloc<Pod>(count);
        if( count > 0 && p == nullptr )
            unexpected_memory_exhaustion();
        return p;
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

}

#endif
