/* Copyright 2015-2020 Zeno Sebastian Endemann <zeno.endemann@googlemail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LIBLBU_ENDIAN_H
#define LIBLBU_ENDIAN_H

#include "lbu/memory.h"

#include <endian.h>
#include <stdint.h>

namespace lbu {

    template< typename T, typename Enable = void >
    struct endian {
        static T from_little(const void* src);
        static T from_big(const void* src);
        static void to_little(T v, void* dst);
        static void to_big(T v, void* dst);
    };

    template<>
    struct endian<uint16_t> {
        static uint16_t from_little(const void* src)
        {
            return le16toh(pod_from_raw_bytes<uint16_t>(src));
        }
        static uint16_t from_big(const void* src)
        {
            return be16toh(pod_from_raw_bytes<uint16_t>(src));
        }
        static void to_little(uint16_t v, void* dst)
        {
            v = htole16(v);
            std::memcpy(dst, &v, sizeof(v));
        }
        static void to_big(uint16_t v, void* dst)
        {
            v = htobe16(v);
            std::memcpy(dst, &v, sizeof(v));
        }
    };

    template< typename T, typename UnsignedType >
    struct endian_unsigned_forwarder {
        typedef T               value_type;
        typedef UnsignedType    unsigned_type;

        static_assert(sizeof(value_type) == sizeof(unsigned_type), "size mismatch");

        static value_type from_little(const void* src)
        {
            return value_reinterpret_cast<unsigned_type, value_type>(endian<unsigned_type>::from_little(src));
        }
        static value_type from_big(const void* src)
        {
            return value_reinterpret_cast<unsigned_type, value_type>(endian<unsigned_type>::from_big(src));
        }
        static void to_little(value_type v, void* dst)
        {
            endian<unsigned_type>::to_little(value_reinterpret_cast<value_type, unsigned_type>(v), dst);
        }
        static void to_big(value_type v, void* dst)
        {
            endian<unsigned_type>::to_big(value_reinterpret_cast<value_type, unsigned_type>(v), dst);
        }
    };

    template<>
    struct endian<int16_t> : public endian_unsigned_forwarder<int16_t, uint16_t> {};

    template<>
    struct endian<uint32_t> {
        static uint32_t from_little(const void* src)
        {
            return le32toh(pod_from_raw_bytes<uint32_t>(src));
        }
        static uint32_t from_big(const void* src)
        {
            return be32toh(pod_from_raw_bytes<uint32_t>(src));
        }
        static void to_little(uint32_t v, void* dst)
        {
            v = htole32(v);
            std::memcpy(dst, &v, sizeof(v));
        }
        static void to_big(uint32_t v, void* dst)
        {
            v = htobe32(v);
            std::memcpy(dst, &v, sizeof(v));
        }
    };

    template<>
    struct endian<int32_t> : public endian_unsigned_forwarder<int32_t, uint32_t> {};

    template<>
    struct endian<uint64_t> {
        static uint64_t from_little(const void* src)
        {
            return le64toh(pod_from_raw_bytes<uint64_t>(src));
        }
        static uint64_t from_big(const void* src)
        {
            return be64toh(pod_from_raw_bytes<uint64_t>(src));
        }
        static void to_little(uint64_t v, void* dst)
        {
            v = htole64(v);
            std::memcpy(dst, &v, sizeof(v));
        }
        static void to_big(uint64_t v, void* dst)
        {
            v = htobe64(v);
            std::memcpy(dst, &v, sizeof(v));
        }
    };

    template<>
    struct endian<int64_t> : public endian_unsigned_forwarder<int64_t, uint64_t> {};

    template<>
    struct endian<float, std::enable_if_t< sizeof(float) == 4 > > : public endian_unsigned_forwarder<float, uint32_t> {};

    template<>
    struct endian<double, std::enable_if_t< sizeof(double) == 8 > > : public endian_unsigned_forwarder<double, uint64_t> {};


    template< typename T >
    T from_little_endian(const void* src)
    {
        return endian<T>::from_little(src);
    }

    template< typename T >
    T from_big_endian(const void* src)
    {
        return endian<T>::from_big(src);
    }

    template< typename T >
    void to_little_endian(T v, void* dst)
    {
        endian<T>::to_little(v, dst);
    }

    template< typename T >
    void to_big_endian(T v, void* dst)
    {
        endian<T>::to_big(v, dst);
    }


    inline uint32_t from_little_endian_u24_packed(const void* src)
    {
        uint32_t v = 0;
        std::memcpy(&v, src, 3);
        return le32toh(v);
    }

    inline int32_t from_little_endian_s24_packed(const void* src)
    {
        uint32_t v = from_little_endian_u24_packed(src);
        if( v >= (uint32_t(1) << 23) )
            v |= uint32_t(0xff) << 24;
        return value_reinterpret_cast<uint32_t, int32_t>(v);
    }

    inline uint32_t from_big_endian_u24_packed(const void* src)
    {
        uint32_t v = 0;
        std::memcpy(reinterpret_cast<char*>(&v) + 1, src, 3);
        return be32toh(v);
    }

    inline int32_t from_big_endian_s24_packed(const void* src)
    {
        uint32_t v = from_big_endian_u24_packed(src);
        if( v >= (uint32_t(1) << 23) )
            v |= uint32_t(0xff) << 24;
        return value_reinterpret_cast<uint32_t, int32_t>(v);
    }

    inline void to_little_endian_u24_packed(uint32_t v, void* dst)
    {
        auto r = htole32(v);
        std::memcpy(dst, &r, 3);
    }

    inline void to_little_endian_s24_packed(int32_t v, void* dst)
    {
        to_little_endian_u24_packed(value_reinterpret_cast<int32_t, uint32_t>(v), dst);
    }

    inline void to_big_endian_u24_packed(uint32_t v, void* dst)
    {
        auto r = htobe32(v);
        std::memcpy(dst, reinterpret_cast<char*>(&r) + 1, 3);
    }

    inline void to_big_endian_s24_packed(int32_t v, void* dst)
    {
        to_big_endian_u24_packed(value_reinterpret_cast<int32_t, uint32_t>(v), dst);
    }

} // namespace lbu

#endif // LIBLBU_ENDIAN_H
