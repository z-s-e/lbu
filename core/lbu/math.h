/* Copyright 2015-2017 Zeno Sebastian Endemann <zeno.endemann@googlemail.com>
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

#ifndef LIBLBU_MATH_H
#define LIBLBU_MATH_H

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
#include <type_traits>

namespace lbu {

    // safe absolute values

    template< typename IntType >
    constexpr typename std::make_unsigned<IntType>::type abs(IntType val)
    {
        static_assert(std::is_integral<IntType>::value, "Need int type");
        static_assert(!std::is_signed<IntType>::value ||
                      (std::numeric_limits<IntType>::max() + std::numeric_limits<IntType>::min() >= IntType(-1)), "Unsupported signed type");
        return val < 0 ? typename std::make_unsigned<IntType>::type(-(val + 1)) + 1 : val;
    }

    template< typename IntType >
    constexpr typename std::make_unsigned<IntType>::type abs_maximum()
    {
        static_assert(std::is_integral<IntType>::value, "Need int type");
        if( !std::is_signed<IntType>::value )
            return std::numeric_limits<IntType>::max();
        else
            return std::max<typename std::make_unsigned<IntType>::type>(std::numeric_limits<IntType>::max(),
                                                                        abs(std::numeric_limits<IntType>::min()));
    }

    // integer log

    template< typename UIntType >
    constexpr UIntType ilog_floor(UIntType base, UIntType val)
    {
        static_assert(std::is_integral<UIntType>::value && !std::is_signed<UIntType>::value, "Need unsigned int type");
        assert( base > 1 && val > 0 );

        UIntType result = 0;
        val /= base;
        while( val > 0 ) {
            ++result;
            val /= base;
        }
        return result;
    }

    template< typename UIntType >
    constexpr UIntType ilog_ceil(UIntType base, UIntType val)
    {
        return val == 1 ? 0 : ilog_floor(base, val - 1) + 1;
    }

    template< typename UIntType >
    constexpr UIntType ilog2_floor(UIntType val)
    {
        return ilog_floor(2, val);
    }

    template< typename UIntType >
    constexpr UIntType ilog2_ceil(UIntType val)
    {
        return val == 1 ? 0 : ilog2_floor(val - 1) + 1;
    }

#ifdef __GNUC__
    template<>
    constexpr unsigned ilog2_floor(unsigned val)
    {
        assert(val > 0);
        return (8 * sizeof(val) - 1) - unsigned(__builtin_clz(val));
    }

    template<>
    constexpr unsigned long ilog2_floor(unsigned long val)
    {
        assert(val > 0);
        return (8 * sizeof(val) - 1) - unsigned(__builtin_clzl(val));
    }

    template<>
    constexpr unsigned long long ilog2_floor(unsigned long long val)
    {
        assert(val > 0);
        return (8 * sizeof(val) - 1) - unsigned(__builtin_clzll(val));
    }

    template<>
    constexpr unsigned ilog2_ceil(unsigned val)
    {
        assert(val > 0);
        return ilog2_floor(val) + (__builtin_popcount(val) > 1 ? 1 : 0);
    }

    template<>
    constexpr unsigned long ilog2_ceil(unsigned long val)
    {
        assert(val > 0);
        return ilog2_floor(val) + (__builtin_popcountl(val) > 1 ? 1 : 0);
    }

    template<>
    constexpr unsigned long long ilog2_ceil(unsigned long long val)
    {
        assert(val > 0);
        return ilog2_floor(val) + (__builtin_popcountll(val) > 1 ? 1 : 0);
    }
#endif


    // integer power

    template< typename BaseType, typename ExpType >
    constexpr BaseType ipow(BaseType base, ExpType exp)
    {
        static_assert(std::is_integral<ExpType>::value && !std::is_signed<ExpType>::value, "Need unsigned int type as exponent");
        static_assert(std::is_integral<BaseType>::value, "Need int type as base");

        BaseType absBase = std::abs(base);
        ExpType expTmp = exp;
        BaseType result = 1;
        while( expTmp ) {
            if (expTmp & 1)
                result *= absBase;
            expTmp >>= 1;
            absBase *= absBase;
        }
        return (base < 0 && (exp & 1)) ? -result : result;
    }

    template< typename UIntType >
    constexpr UIntType next_greater_pow2(UIntType val)
    {
        static_assert(std::is_integral<UIntType>::value && !std::is_signed<UIntType>::value, "Need unsigned int type");
        assert(val <= std::numeric_limits<UIntType>::max() / 2);

        for( unsigned i = 1; i < (8 * sizeof(val)); i <<= 1 )
            val |= (val >> i);
        return val + 1;
    }

#ifdef __GNUC__
    template<>
    constexpr unsigned next_greater_pow2(unsigned val)
    {
        assert(val <= std::numeric_limits<unsigned>::max() / 2);
        return val ? (2u << ilog2_floor(val)) : 1;
    }

    template<>
    constexpr unsigned long next_greater_pow2(unsigned long val)
    {
        assert(val <= std::numeric_limits<unsigned long>::max() / 2);
        return val ? (2ul << ilog2_floor(val)) : 1;
    }

    template<>
    constexpr unsigned long long next_greater_pow2(unsigned long long val)
    {
        assert(val <= std::numeric_limits<unsigned long long>::max() / 2);
        return val ? (2ull << ilog2_floor(val)) : 1;
    }
#endif

    template< typename UIntType >
    constexpr bool is_pow2(UIntType val)
    {
        static_assert(std::is_integral<UIntType>::value && !std::is_signed<UIntType>::value, "Need unsigned int type");
        return val ? (!(val & (val - 1))) : false;
    }
}

#endif // LIBLBU_MATH_H
