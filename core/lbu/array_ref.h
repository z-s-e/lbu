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

#ifndef LIBLBU_ARRAY_REF_H
#define LIBLBU_ARRAY_REF_H

#include "lbu/align.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <limits>
#include <type_traits>
#include <utility>

namespace lbu {

    template< typename T, typename SizeType = size_t >
    class array_ref {
    public:
        typedef T                                               value_type;
        typedef std::add_const_t<T>                             const_value_type;
        typedef std::add_lvalue_reference_t<value_type>         reference;
        typedef std::add_lvalue_reference_t<const_value_type>   const_reference;
        typedef SizeType                                        size_type;
        typedef ptrdiff_t                                       difference_type;
        typedef std::add_pointer_t<value_type>                  pointer;
        typedef std::add_pointer_t<const_value_type>            const_pointer;
        typedef pointer                                         iterator;
        typedef const_pointer                                   const_iterator;

        array_ref() = default; // default copy ctor/assignment ok

        template< size_t Size >
        array_ref(T (&array)[Size]) : d(array), n(size_type(Size))
        {
            static_assert(Size <= std::numeric_limits<size_type>::max(), "array too big");
        }

        template< size_t Size >
        array_ref(std::array<T, Size>& array) : d(array.data()), n(size_type(Size))
        {
            static_assert(Size <= std::numeric_limits<size_type>::max(), "array too big");
        }

        template<typename U = T,
                 typename V = typename std::enable_if_t<std::is_const<U>::value || std::is_volatile<U>::value, std::remove_cv_t<U>>>
        array_ref(array_ref<V, SizeType> array) : d(array.data()), n(array.size())
        {
        }

        array_ref(pointer data, size_type size) : d(data), n(size) {}

        array_ref(pointer begin, pointer end) : d(begin), n(size_type(end - begin))
        {
            assert(end >= begin && size_t(end - begin) <= size_t(std::numeric_limits<size_type>::max()));
        }

        pointer data() { return d; }
        size_type size() const { return n; }
        size_t byte_size() const { return size_t(n) * sizeof(T); }

        explicit operator bool() const { return n > 0 && d; }

        iterator begin() { return d; }
        iterator end() { return d + n; }

        const_iterator begin() const { return d; }
        const_iterator end() const { return d + n; }
        const_iterator cbegin() const { return begin(); }
        const_iterator cend() const { return end(); }

        reference operator[](size_type idx)
        {
            assert(idx < n);
            return *(d + idx);
        }
        const_reference operator[](size_type idx) const
        {
            return at(idx);
        }
        const_reference at(size_type idx) const
        {
            assert(idx < n);
            return *(d + idx);
        }

        array_ref<T, SizeType> sub(size_type start, size_type size = std::numeric_limits<size_type>::max()) const
        {
            assert(start <= n);
            return array_ref<T, SizeType>(d + start, std::min(size, n - start));
        }
        array_ref<T, SizeType> sub_first(size_type size) const { return sub(0, size); }
        array_ref<T, SizeType> sub_last(size_type size) const { return sub(n - std::min(size, n), size); }

        std::pair<array_ref<T, SizeType>, array_ref<T, SizeType>> split(size_type idx) const
        {
            return { {d, idx}, sub(idx) };
        }

    private:
        pointer d = {};
        size_type n = 0;
    };

    template< typename T >
    array_ref<T> array_ref_one_element(T* element)
    {
        return array_ref<T>(element, 1);
    }

    template< typename T, typename SizeType >
    void array_ref_content_copy(array_ref<T, SizeType> dst, array_ref<const T, SizeType> src)
    {
        assert(dst.size() >= src.size());
        std::copy(src.begin(), src.end(), dst.begin());
    }

    template< typename T, typename SizeType >
    void array_ref_content_move(array_ref<T, SizeType> dst, array_ref<T, SizeType> src)
    {
        assert(dst.size() >= src.size());
        std::move(src.begin(), src.end(), dst.begin());
    }

    template< typename T, typename SizeType >
    bool array_ref_content_equal(array_ref<const T, SizeType> a1, array_ref<const T, SizeType> a2)
    {
        if( a1.size() != a2.size() )
            return false;
        return std::equal(a1.begin(), a1.end(), a2.begin());
    }


    template<>
    class array_ref<void> {
    public:
        typedef void                value_type;
        typedef size_t              size_type;
        typedef ptrdiff_t           difference_type;

        array_ref() = default; // default copy ctor/assignment ok

        template< typename T, typename SizeType >
        array_ref(array_ref<T, SizeType> array) : d(array.data()), n(array.byte_size())
        {
            static_assert(std::is_trivial<T>::value, "only trivial types are safe for type removal");
        }

        value_type* data() { return d; }
        size_type size() const { return n; }
        size_t byte_size() const { return n; }

        explicit operator bool() const { return n > 0 && d; }

        template< typename T >
        array_ref<T> array_static_cast()
        {
            assert(is_aligned<T>(d));
            assert((n % sizeof(T)) == 0);
            return {static_cast<T*>(d), n / sizeof(T)};
        }

    private:
        value_type* d = {};
        size_type n = 0;
    };

    template<>
    class array_ref<const void> {
    public:
        typedef const void          value_type;
        typedef size_t              size_type;
        typedef ptrdiff_t           difference_type;

        array_ref() = default; // default copy ctor/assignment ok

        template< typename T, typename SizeType >
        array_ref(array_ref<T, SizeType> array) : d(array.data()), n(array.byte_size())
        {
            static_assert(std::is_trivial<T>::value || std::is_void<T>::value, "only trivial types are safe for type removal");
        }

        value_type* data() { return d; }
        size_type size() const { return n; }
        size_t byte_size() const { return n; }

        explicit operator bool() const { return n > 0 && d; }

        template< typename T >
        array_ref<const T> array_static_cast()
        {
            assert(is_aligned<T>(d));
            assert((n % sizeof(T)) == 0);
            return {static_cast<const T*>(d), n / sizeof(T)};
        }

    private:
        value_type* d = {};
        size_type n = 0;
    };


} // namespace lbu

#endif // LIBLBU_ARRAY_REF_H

