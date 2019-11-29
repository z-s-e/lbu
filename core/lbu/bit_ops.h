/* Copyright 2019 Zeno Sebastian Endemann <zeno.endemann@googlemail.com>
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

