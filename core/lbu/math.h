/* Copyright 2015-2020 Zeno Sebastian Endemann <zeno.endemann@mailbox.org>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LIBLBU_MATH_H
#define LIBLBU_MATH_H

#include <algorithm>
#include <cassert>
#include <limits>
#include <type_traits>

namespace lbu {

    // safe absolute values

    template< typename IntType >
    constexpr typename std::make_unsigned<IntType>::type abs(IntType val)
    {
        static_assert(std::is_integral_v<IntType>);
        static_assert(std::is_unsigned_v<IntType>
                      || (std::numeric_limits<IntType>::max() + std::numeric_limits<IntType>::min() >= IntType(-1)), "unsupported type");
        using U = typename std::make_unsigned<IntType>::type;
        return val < 0 ? U(-(val + 1)) + 1 : U(val);
    }

    template< typename IntType >
    constexpr typename std::make_unsigned<IntType>::type abs_maximum()
    {
        static_assert(std::is_integral_v<IntType>);
        if( ! std::is_signed<IntType>::value )
            return std::numeric_limits<IntType>::max();
        else
            return std::max<typename std::make_unsigned<IntType>::type>(std::numeric_limits<IntType>::max(),
                                                                        abs(std::numeric_limits<IntType>::min()));
    }

    // integer div

    template< typename IntType >
    constexpr IntType idiv_ceil(IntType num, IntType den)
    {
        static_assert(std::is_integral_v<IntType>, "need int type");
        return (num / den) + (num % den > 0 ? 1 : 0);
    }

    template< typename IntType >
    constexpr IntType idiv_floor(IntType num, IntType den)
    {
        static_assert(std::is_integral_v<IntType>, "need int type");
        return (num / den) - (num % den < 0 ? 1 : 0);
    }

    // integer log

    template< typename UIntType >
    constexpr UIntType ilog_floor(UIntType base, UIntType val)
    {
        static_assert(std::is_integral_v<UIntType> && ! std::is_signed_v<UIntType>, "need unsigned int type");
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
        static_assert(std::is_integral_v<ExpType> && ! std::is_signed_v<ExpType>, "need unsigned int type as exponent");
        static_assert(std::is_integral_v<BaseType>, "need int type as base");

        BaseType abs_base = abs(base);
        ExpType exp_tmp = exp;
        BaseType result = 1;
        while( exp_tmp ) {
            if( exp_tmp & 1 )
                result *= abs_base;
            exp_tmp >>= 1;
            abs_base *= abs_base;
        }
        return (base < 0 && (exp & 1)) ? -result : result;
    }

    template< typename UIntType >
    constexpr UIntType next_greater_pow2(UIntType val)
    {
        static_assert(std::is_integral_v<UIntType> && ! std::is_signed_v<UIntType>);
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
        static_assert(std::is_integral_v<UIntType> && ! std::is_signed_v<UIntType>);
        return val ? ( ! (val & (val - 1))) : false;
    }

}

#endif
