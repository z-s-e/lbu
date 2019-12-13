/* Copyright 2019 Zeno Sebastian Endemann <zeno.endemann@googlemail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LIBLBU_BIT_OPS_H
#define LIBLBU_BIT_OPS_H

#include "lbu/array_ref.h"

#include <type_traits>

namespace lbu {
namespace bit_ops {

    template< typename IntType >
    constexpr IntType flag_set(IntType old_flags, IntType flag, bool value)
    {
        static_assert(std::is_integral<IntType>::value, "Need int type");
        if( value )
            return old_flags | flag;
        else
            return old_flags & (~flag);
    }

    template< typename IntType >
    constexpr bool flag_get(IntType flags, IntType flag)
    {
        static_assert(std::is_integral<IntType>::value, "Need int type");
        return (flags & flag) != 0;
    }

} // namespace bit_ops
} // namespace lbu

#endif // LIBLBU_BIT_OPS_H

