/* Copyright 2015-2019 Zeno Sebastian Endemann <zeno.endemann@googlemail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LIBLBU_C_UTILS_FUNCTION_H
#define LIBLBU_C_UTILS_FUNCTION_H

#include <utility>

namespace lbu {

    /// Function pointer functor wrapper with less overhead than std::function.
    template< typename R, typename ...Args >
    struct function_ptr {
        using Function = R (*)(Args...);

        template< Function F >
        struct static_functor {
            R operator()(Args... args) const { return F(args...); }
        };

        function_ptr() = default;
        function_ptr(Function fn) : function(fn) {}

        bool is_valid() const { return function != nullptr; }
        explicit operator bool() const { return is_valid(); }

        R operator()(Args... args) const { return (*function)(args...); }

        Function function = {};
    };

    template< typename ...Args >
    struct callback {
        using Function = void (*)(Args..., void*);

        callback() = default;
        callback(Function fn, void* data = nullptr) : function(fn), userdata(data) {}

        explicit operator bool() const { return function != nullptr; }
        void operator()(Args... args) const { (*function)(args..., userdata); }

        Function function = {};
        void* userdata = {};


        // convenience

        template< class T >
        static void functor_dispatch(Args... args, void* userdata)
        {
            T* obj = static_cast<T*>(userdata);
            (*obj)(args...);
        }
        template< class T >
        static callback functor_callback(T* functor) { return {functor_dispatch<T>, functor}; }

        template< class T, void(T::*Member)(Args...) >
        static void member_dispatch(Args... args, void* userdata)
        {
            T* obj = static_cast<T*>(userdata);
            (obj->*Member)(args...);
        }
        template< class T, void(T::*Member)(Args...) >
        static callback member_callback(T* object) { return {member_dispatch<T, Member>, object}; }
    };

    template< typename R, typename ...Args >
    struct callback_r {
        using Function = R (*)(Args..., void*);

        callback_r() = default;
        callback_r(Function fn, void* data = nullptr) : function(fn), userdata(data) {}

        explicit operator bool() const { return function != nullptr; }
        R operator()(Args... args) const { return (*function)(args..., userdata); }

        Function function = {};
        void* userdata = {};


        // convenience

        template< class T >
        static R functor_dispatch(Args... args, void* userdata)
        {
            T* obj = static_cast<T*>(userdata);
            return (*obj)(args...);
        }
        template< class T >
        static callback_r functor_callback(T* functor) { return {functor_dispatch<T>, functor}; }

        template< class T, R(T::*Member)(Args...) >
        static R member_dispatch(Args... args, void* userdata)
        {
            T* obj = static_cast<T*>(userdata);
            return (obj->*Member)(args...);
        }
        template< class T, void(T::*Member)(Args...) >
        static callback_r member_callback(T* object) { return {member_dispatch<T, Member>, object}; }
    };

} // namespace lbu

#endif // LIBLBU_C_UTILS_FUNCTION_H

