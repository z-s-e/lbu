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

#ifndef LIBLBU_ASCII_H
#define LIBLBU_ASCII_H

#include "lbu/array_ref.h"
#include "lbu/math.h"

namespace lbu {
namespace ascii {

    constexpr bool is_ascii(char c)
    {
        return c >= 0x00 && c <= 0x7F;
    }

    constexpr bool is_cntrl(char c)
    {
        return c <= 0x1F || c == 0x7F;
    }

    constexpr bool is_print(char c)
    {
        return c >= 0x20 && c <= 0x7E;
    }

    constexpr bool is_space(char c)
    {
        return c == 0x20 || (c >= 0x09 && c <= 0x0D);
    }

    constexpr bool is_blank(char c)
    {
        return c == 0x20 || c == 0x09;
    }

    constexpr bool is_graph(char c)
    {
        return c >= 0x21 && c <= 0x7E;
    }

    constexpr bool is_upper(char c)
    {
        return c >= 0x41 && c <= 0x5A;
    }

    constexpr bool is_lower(char c)
    {
        return c >= 0x61 && c <= 0x7A;
    }

    constexpr bool is_alpha(char c)
    {
        return is_upper(c) || is_lower(c);
    }

    constexpr bool is_digit(char c)
    {
        return c >= 0x30 && c <= 0x39;
    }

    constexpr bool is_alnum(char c)
    {
        return is_alpha(c) || is_digit(c);
    }

    constexpr bool is_punct(char c)
    {
        return is_graph(c) && !is_alnum(c);
    }

    constexpr bool is_hexdigit(char c)
    {
        return is_digit(c) || (c >= 0x41 && c <= 0x46) || (c >= 0x61 && c <= 0x66);
    }

    constexpr unsigned char hexdigit_value(char c)
    {
        if( c >= 0x30 && c <= 0x39 )
            return c - 0x30;
        else if( c >= 0x41 && c <= 0x46 )
            return c - 0x41 + 0x0A;
        else if( c >= 0x61 && c <= 0x66 )
            return c - 0x61 + 0x0A;
        else
            assert(false);
        return 0xFF;
    }

    constexpr char to_lower(char c)
    {
        return ( c >= 0x41 && c <= 0x5A ) ? c + 0x20 : c;
    }

    constexpr char to_upper(char c)
    {
        return ( c >= 0x61 && c <= 0x7A ) ? c - 0x20 : c;
    }


    class integer {
    public:
        enum class LeadingZeros {
            Accept,
            Reject
        };

        enum class NegativeZero {
            Accept,
            Reject
        };


        template< typename T >
        static constexpr bool from_decimal(const char* str,
                                           T* result = nullptr,
                                           LeadingZeros lz = LeadingZeros::Reject,
                                           NegativeZero nz = NegativeZero::Reject);


        template< typename T >
        class decimal {
        public:
            constexpr decimal() = default;
            constexpr decimal(T value);

            array_ref<const char> ref() const { return {data(), MaxDataSize - begin - 1}; }
            const char* data() const { return d + begin; }

            static constexpr size_t max_size() { return MaxDataSize; }

        private:
            static_assert(std::is_integral<T>::value, "Need integral type");
            using U = typename std::make_unsigned<T>::type;

            static constexpr size_t MaxDataSize =  1 // null terminator
                                                 + 1 // sign
                                                 + 1 + ilog_floor<U>(10, abs_maximum<T>()); // digits
            static_assert(MaxDataSize < 256, "Unsupported integer type");

            char d[MaxDataSize] = {};
            unsigned char begin = MaxDataSize - 1;
        };
    };


    // implementation

    template< typename T >
    constexpr bool integer::from_decimal(const char* str,
                                         T* result,
                                         LeadingZeros lz,
                                         NegativeZero nz)

    {
        static_assert(std::is_integral<T>::value, "Need integral type");

        using U = typename std::make_unsigned<T>::type;

        U absVal = 0;
        const char* c = str;
        if( c == nullptr )
            return false;

        bool neg = false;
        if( *c == 0x2D ) {
            neg = true;
            ++c;
        }

        if( *c == 0 )
            return false;

        if( *c == 0x30 ) {
            ++c;
            if( lz == LeadingZeros::Reject && *c != 0 )
                return false;

            while( *c == 0x30 )
                ++c;
        }

        const U absValMax = neg ? abs(std::numeric_limits<T>::min())
                                : std::numeric_limits<T>::max();

        while( *c ) {
            if( ! is_digit(*c) )
                return false;

            int v = *c - 0x30;
            if( (absValMax / 10) < absVal )
                return false;
            absVal *= 10;
            if( absValMax - v < absVal )
                return false;
            absVal += v;

            ++c;
        }

        if( absVal == 0 && neg && nz == NegativeZero::Reject )
            return false;

        if( result )
            *result = neg ? -T(absVal) : absVal;
        return true;
    }

    template< typename T >
    constexpr integer::decimal<T>::decimal(T value)
    {
        char* p = d + MaxDataSize - 1;
        *p = 0;

        if( value == 0 ) {
            --p;
            *p = 0x30;
        }

        const bool neg = (value < 0);
        U u = abs(value);

        while( u > 0 ) {
            --p;
            *p = (u % 10) + 0x30;
            u /= 10;
        }

        if( neg ) {
            --p;
            *p = 0x2D;
        }

        begin = p - d;
    }
}
}

#endif // LIBLBU_ASCII_H
