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

#ifndef LIBLBU_ALSA_H
#define LIBLBU_ALSA_H

#include "lbu/memory.h"
#include "lbu/endian.h"

#include <alsa/asoundlib.h>
#include <cstring>

namespace lbu {
namespace alsa {

    class card_list {
    public:
        class element_ptr {
        public:
            element_ptr();
            // default copy ctor/assignment ok

            bool is_valid() const { return idx >= 0; }
            explicit operator bool() const { return is_valid(); }
            int last_error() const { return err; }

            int index() const { return idx; }
            unique_cstr name();
            unique_cstr longname();

            element_ptr& operator++();

        private:
            int idx;
            int err;
        };

        static element_ptr first() { return {}; }

        static int index_for_name(const char* name);
        static int index_for_longname(const char* longname);
    };


    class device_list {
    public:
        enum AllCardsIndex {
            AllCards = -1
        };

        enum Direction {
            Playback = 1 << 0,
            Capture = 1 << 1,
            Both = Playback | Capture
        };

        enum Flags {
            FlagsNone = 0,
            FlagsDirectHardware = 1 << 1,
            FlagsAutoConverting = 1 << 2,
            FlagsCompatibility = 1 << 3
        };

        explicit device_list(int cardIndex = AllCards, int* error = nullptr);
        ~device_list();

        device_list(const device_list&) = delete;
        device_list& operator=(const device_list&) = delete;

        class element_ptr {
        public:
            explicit element_ptr(const device_list& list);
            // default copy ctor/assignment ok

            bool is_valid() const { return *p != nullptr; }
            explicit operator bool() const { return is_valid(); }

            unique_cstr name() const;
            unique_cstr description() const;
            Direction direction() const;

            element_ptr& operator++();

        private:
            void** p = {};
        };

        element_ptr first() const { return element_ptr(*this); }

        static int device_flags(const char* name);

        struct hw_device { char name[8]; };
        static hw_device device_for_card_index(int index);

    private:
        void** hints = {};
    };


    class pcm_format {
    public:
        pcm_format() = default;
        pcm_format(snd_pcm_format_t type, int16_t channels, int32_t rate)
            : m_type(type), m_channels(channels), m_rate(rate) {}
        // default copy ctor/assignment ok

        snd_pcm_format_t type() const { return static_cast<snd_pcm_format_t>(m_type); }
        int16_t channels() const { return m_channels; }
        int32_t rate() const { return m_rate; }

        int frame_native_byte_size() const { return native_byte_size(type()) * channels(); }
        int frame_packed_byte_size() const { return packed_byte_size(type()) * channels(); }

        static int native_byte_size(snd_pcm_format_t type);
        static int packed_byte_size(snd_pcm_format_t type);

    private:
        int16_t m_type = SND_PCM_FORMAT_UNKNOWN;
        int16_t m_channels = 0;
        int32_t m_rate = 0;
    };

    class pcm_config {
    public:
        pcm_config() = default;
        pcm_config(snd_pcm_access_t access, pcm_format format, snd_pcm_uframes_t framesPerPeriod, uint32_t periods)
            : m_access(access), m_format(format), m_period_size(uint32_t(framesPerPeriod)), m_periods(periods) {}
        // default copy ctor/assignment ok

        snd_pcm_access_t access() const { return m_access; }
        pcm_format format() const { return m_format; }
        snd_pcm_uframes_t frames_per_period() const { return snd_pcm_uframes_t(m_period_size); }
        uint32_t periods() const { return m_periods; }

        bool is_interleaved() const;
        bool is_noninterleaved() const;

        int period_native_byte_size() const { return format().frame_native_byte_size() * int(frames_per_period()); }
        int period_packed_byte_size() const { return format().frame_packed_byte_size() * int(frames_per_period()); }

    private:
        snd_pcm_access_t m_access;
        pcm_format m_format;
        uint32_t m_period_size;
        uint32_t m_periods;
    };


    class pcm_buffer {
    public:
        pcm_buffer() = default;
        pcm_buffer(const snd_pcm_channel_area_t* areas, snd_pcm_uframes_t offset, snd_pcm_uframes_t frames)
            : m_areas(areas), m_offset(offset), m_frames(frames) {}
        // default copy ctor/assignment ok

        const snd_pcm_channel_area_t* areas() const { return m_areas; }
        snd_pcm_uframes_t offset() const { return m_offset; }
        snd_pcm_uframes_t frames() const { return m_frames; }

        class channel_iter {
        public:
            channel_iter() = default;
            channel_iter(void* data, unsigned stride)
                : d(data), stride(stride) {}
            // default copy ctor/assignment ok

            void* data() { return d; }
            const void* data() const { return d; }

            channel_iter& operator++();
            channel_iter& operator--();

            int16_t read_i16(snd_pcm_format_t type) const;
            int32_t read_i32(snd_pcm_format_t type) const;
            float read_f(snd_pcm_format_t type) const;

            template< typename T >
            T read(snd_pcm_format_t type) const;

            void write_i16(int16_t value, snd_pcm_format_t type);
            void write_i32(int32_t value, snd_pcm_format_t type);
            void write_f(float value, snd_pcm_format_t type);

            template< typename T >
            void write(T value, snd_pcm_format_t type);

        private:
            void* d = nullptr;
            unsigned stride = 0;
        };

        channel_iter begin(unsigned channel) const;

    private:
        friend class pcm_device;

        const snd_pcm_channel_area_t* m_areas = {};
        snd_pcm_uframes_t m_offset = 0;
        snd_pcm_uframes_t m_frames = 0;
    };


    class pcm_device {
    public:
        enum Flags {
            FlagsNone = 0,
            FlagsNonBlock = SND_PCM_NONBLOCK,
            FlagsNoAutoResample = SND_PCM_NO_AUTO_RESAMPLE,
            FlagsNoAutoChannels = SND_PCM_NO_AUTO_CHANNELS,
            FlagsNoAutoFormat =  SND_PCM_NO_AUTO_FORMAT,
            FlagsNoAutoConversion = SND_PCM_NO_AUTO_RESAMPLE | SND_PCM_NO_AUTO_CHANNELS | SND_PCM_NO_AUTO_CHANNELS
        };

        pcm_device(const char* deviceName, snd_pcm_stream_t dir, int flags, int* error = nullptr);

        pcm_device(const pcm_device&) = delete;
        pcm_device& operator=(const pcm_device&) = delete;

        ~pcm_device() { cleanup(); }

        int open(const char* deviceName, snd_pcm_stream_t dir, int flags);
        void close();

        bool is_valid() const { return m_handle; }
        explicit operator bool() const { return is_valid(); }

        snd_pcm_t* handle() const { return m_handle; }

        pcm_buffer mmap_begin(snd_pcm_uframes_t frames, int* error);
        snd_pcm_sframes_t mmap_commit(const pcm_buffer& buffer);

    private:
        void cleanup() { if( m_handle ) snd_pcm_close(m_handle); }

        snd_pcm_t* m_handle = {};
    };


    // implementation

    inline card_list::element_ptr::element_ptr() : idx(-1)
    {
        err = snd_card_next(&idx);
    }

    inline unique_cstr card_list::element_ptr::name()
    {
        char* r = {};
        err = snd_card_get_name(idx, &r);
        return unique_cstr(r);
    }

    inline unique_cstr card_list::element_ptr::longname()
    {
        char* r = {};
        err = snd_card_get_longname(idx, &r);
        return unique_cstr(r);
    }

    inline card_list::element_ptr& card_list::element_ptr::operator++()
    {
        err = snd_card_next(&idx);
        return *this;
    }

    inline int card_list::index_for_name(const char* name)
    {
        for( auto c = first(); c.is_valid(); ++c ) {
            if( std::strcmp(name, c.name().get()) == 0)
                return c.index();
        }
        return -1;
    }

    inline int card_list::index_for_longname(const char* longname)
    {
        for( auto c = first(); c.is_valid(); ++c ) {
            if( std::strcmp(longname, c.longname().get()) == 0)
                return c.index();
        }
        return -1;
    }

    inline device_list::device_list(int cardIndex, int* error)
    {
        int err = snd_device_name_hint(cardIndex, "pcm", &hints);
        if( error )
            *error = err;
    }

    inline device_list::~device_list()
    {
        if( hints )
            snd_device_name_free_hint(hints);
    }

    inline int device_list::device_flags(const char* name)
    {
        switch( name[0] ) {
        case 'h':
            if( std::strncmp(name + 1, "w:", 2) == 0 )
                return FlagsDirectHardware;
            return 0;
        case 'p':
            if( std::strncmp(name + 1, "lug:", 4) == 0 )
                return FlagsAutoConverting;
            if( std::strncmp(name + 1, "lughw:", 6) == 0 )
                return FlagsAutoConverting;
            return 0;
        case 'd':
            if( std::strncmp(name + 1, "mix:", 4) == 0 )
                return FlagsCompatibility;
            if( std::strncmp(name + 1, "snoop:", 6) == 0 )
                return FlagsCompatibility;
            return 0;
        default:
            return 0;
        }
    }

    inline device_list::hw_device device_list::device_for_card_index(int index)
    {
        assert(index < 10000);
        hw_device dev;
        ::snprintf(dev.name, sizeof(dev.name), "hw:%d", index);
        return dev;
    }

    inline device_list::element_ptr::element_ptr(const device_list& list)
    {
        static void* null_ptr = {};
        p = list.hints != nullptr ? list.hints : &null_ptr;
    }

    inline unique_cstr device_list::element_ptr::name() const
    {
        return unique_cstr(snd_device_name_get_hint(*p, "NAME"));
    }

    inline unique_cstr device_list::element_ptr::description() const
    {
        return unique_cstr(snd_device_name_get_hint(*p, "DESC"));
    }

    inline device_list::Direction device_list::element_ptr::direction() const
    {
        unique_cstr s(snd_device_name_get_hint(*p, "IOID"));

        if( s.get() == nullptr )
            return Direction::Both;
        else if( std::strcmp(s.get(), "Output") == 0 )
            return Direction::Playback;
        else
            return Direction::Capture;
    }

    inline device_list::element_ptr& device_list::element_ptr::operator++()
    {
        ++p;
        return *this;
    }

    inline int pcm_format::native_byte_size(snd_pcm_format_t type)
    {
        switch( type ) {
        case SND_PCM_FORMAT_S8:
        case SND_PCM_FORMAT_U8:
            return 1;
        case SND_PCM_FORMAT_S16_LE:
        case SND_PCM_FORMAT_S16_BE:
        case SND_PCM_FORMAT_U16_LE:
        case SND_PCM_FORMAT_U16_BE:
            return 2;
        case SND_PCM_FORMAT_S24_LE:
        case SND_PCM_FORMAT_S24_BE:
        case SND_PCM_FORMAT_U24_LE:
        case SND_PCM_FORMAT_U24_BE:
        case SND_PCM_FORMAT_S32_LE:
        case SND_PCM_FORMAT_S32_BE:
        case SND_PCM_FORMAT_U32_LE:
        case SND_PCM_FORMAT_U32_BE:
        case SND_PCM_FORMAT_FLOAT_LE:
        case SND_PCM_FORMAT_FLOAT_BE:
        case SND_PCM_FORMAT_S24_3LE:
        case SND_PCM_FORMAT_S24_3BE:
        case SND_PCM_FORMAT_U24_3LE:
        case SND_PCM_FORMAT_U24_3BE:
        case SND_PCM_FORMAT_S20_3LE:
        case SND_PCM_FORMAT_S20_3BE:
        case SND_PCM_FORMAT_U20_3LE:
        case SND_PCM_FORMAT_U20_3BE:
        case SND_PCM_FORMAT_S18_3LE:
        case SND_PCM_FORMAT_S18_3BE:
        case SND_PCM_FORMAT_U18_3LE:
        case SND_PCM_FORMAT_U18_3BE:
            return 4;
        case SND_PCM_FORMAT_FLOAT64_LE:
        case SND_PCM_FORMAT_FLOAT64_BE:
            return 8;
        default:
            assert(false);
            return 0;
        }
    }

    inline int pcm_format::packed_byte_size(snd_pcm_format_t type)
    {
        switch( type ) {
        case SND_PCM_FORMAT_S8:
        case SND_PCM_FORMAT_U8:
            return 1;
        case SND_PCM_FORMAT_S16_LE:
        case SND_PCM_FORMAT_S16_BE:
        case SND_PCM_FORMAT_U16_LE:
        case SND_PCM_FORMAT_U16_BE:
            return 2;
        case SND_PCM_FORMAT_S24_LE:
        case SND_PCM_FORMAT_S24_BE:
        case SND_PCM_FORMAT_U24_LE:
        case SND_PCM_FORMAT_U24_BE:
        case SND_PCM_FORMAT_S32_LE:
        case SND_PCM_FORMAT_S32_BE:
        case SND_PCM_FORMAT_U32_LE:
        case SND_PCM_FORMAT_U32_BE:
        case SND_PCM_FORMAT_FLOAT_LE:
        case SND_PCM_FORMAT_FLOAT_BE:
            return 4;
        case SND_PCM_FORMAT_FLOAT64_LE:
        case SND_PCM_FORMAT_FLOAT64_BE:
            return 8;
        case SND_PCM_FORMAT_S24_3LE:
        case SND_PCM_FORMAT_S24_3BE:
        case SND_PCM_FORMAT_U24_3LE:
        case SND_PCM_FORMAT_U24_3BE:
        case SND_PCM_FORMAT_S20_3LE:
        case SND_PCM_FORMAT_S20_3BE:
        case SND_PCM_FORMAT_U20_3LE:
        case SND_PCM_FORMAT_U20_3BE:
        case SND_PCM_FORMAT_S18_3LE:
        case SND_PCM_FORMAT_S18_3BE:
        case SND_PCM_FORMAT_U18_3LE:
        case SND_PCM_FORMAT_U18_3BE:
            return 3;
        default:
            assert(false);
            return 0;
        }
    }

    inline bool pcm_config::is_interleaved() const
    {
        return m_access == SND_PCM_ACCESS_MMAP_INTERLEAVED || m_access == SND_PCM_ACCESS_RW_INTERLEAVED;
    }

    inline bool pcm_config::is_noninterleaved() const
    {
        return m_access == SND_PCM_ACCESS_MMAP_NONINTERLEAVED || m_access == SND_PCM_ACCESS_RW_NONINTERLEAVED;
    }

    inline pcm_buffer::channel_iter pcm_buffer::begin(unsigned channel) const
    {
        assert((m_areas[channel].first & 7) == 0);
        assert((m_areas[channel].step & 7) == 0);

        const unsigned stride = (m_areas[channel].step >> 3);
        char* addr = static_cast<char*>(m_areas[channel].addr);
        addr += (m_areas[channel].first >> 3);
        addr += stride * m_offset;
        return {addr, stride};
    }

    inline pcm_buffer::channel_iter& pcm_buffer::channel_iter::operator++()
    {
        d = static_cast<char*>(d) + stride;
        return *this;
    }

    inline pcm_buffer::channel_iter& pcm_buffer::channel_iter::operator--()
    {
        d = static_cast<char*>(d) - stride;
        return *this;
    }

    inline int16_t pcm_buffer::channel_iter::read_i16(snd_pcm_format_t type) const
    {
        int16_t r = 0;

        switch( type ) {
        case SND_PCM_FORMAT_S16_BE:
            r = from_big_endian<int16_t>(d);
            break;
        case SND_PCM_FORMAT_S16_LE:
            r = from_little_endian<int16_t>(d);
            break;
        default:
            assert(false);
        }

        return r;
    }

    inline int32_t pcm_buffer::channel_iter::read_i32(snd_pcm_format_t type) const
    {
        switch( type ) {
        case SND_PCM_FORMAT_S16_BE:
        case SND_PCM_FORMAT_S16_LE:
            return read_i16(type);
        case SND_PCM_FORMAT_S24_LE:
        case SND_PCM_FORMAT_S24_3LE:
            return from_little_endian_s24_packed(d);
        case SND_PCM_FORMAT_S20_3LE: {
            auto v = from_little_endian_u24_packed(d);
            if( v >= (uint32_t(1) << 19) )
                v |= uint32_t(0xfff) << 20;
            return value_reinterpret_cast<uint32_t, int32_t>(v);
        }
        case SND_PCM_FORMAT_S18_3LE: {
            auto v = from_little_endian_u24_packed(d);
            if( v >= (uint32_t(1) << 17) )
                v |= uint32_t(0xfffc) << 16;
            return value_reinterpret_cast<uint32_t, int32_t>(v);
        }
        case SND_PCM_FORMAT_S24_BE:
            return from_big_endian_s24_packed(static_cast<const char*>(d) + 1);
        case SND_PCM_FORMAT_S24_3BE:
            return from_big_endian_s24_packed(d);
        case SND_PCM_FORMAT_S20_3BE: {
            auto v = from_big_endian_u24_packed(d);
            if( v >= (uint32_t(1) << 19) )
                v |= uint32_t(0xfff) << 20;
            return value_reinterpret_cast<uint32_t, int32_t>(v);
        }
        case SND_PCM_FORMAT_S18_3BE: {
            auto v = from_big_endian_u24_packed(d);
            if( v >= (uint32_t(1) << 17) )
                v |= uint32_t(0xfffc) << 16;
            return value_reinterpret_cast<uint32_t, int32_t>(v);
        }
        case SND_PCM_FORMAT_S32_LE:
            return from_little_endian<int32_t>(d);
        case SND_PCM_FORMAT_S32_BE:
            return from_big_endian<int32_t>(d);
        default:
            assert(false);
            return 0;
        }
    }

    inline float pcm_buffer::channel_iter::read_f(snd_pcm_format_t type) const
    {
        switch( type ) {
        case SND_PCM_FORMAT_FLOAT_BE:
            return from_big_endian<float>(d);
        case SND_PCM_FORMAT_FLOAT_LE:
            return from_little_endian<float>(d);
        default:
            assert(false);
            return 0;
        }
    }

    template<>
    int16_t pcm_buffer::channel_iter::read<int16_t>(snd_pcm_format_t type) const
    {
        return read_i16(type);
    }

    template<>
    int32_t pcm_buffer::channel_iter::read<int32_t>(snd_pcm_format_t type) const
    {
        return read_i32(type);
    }

    template<>
    float pcm_buffer::channel_iter::read<float>(snd_pcm_format_t type) const
    {
        return read_f(type);
    }

    inline void pcm_buffer::channel_iter::write_i16(int16_t value, snd_pcm_format_t type)
    {
        switch( type ) {
        case SND_PCM_FORMAT_S16_BE:
            to_big_endian(value, d);
            break;
        case SND_PCM_FORMAT_S16_LE:
            to_little_endian(value, d);
            break;
        default:
            assert(false);
        }
    }

    inline void pcm_buffer::channel_iter::write_i32(int32_t value, snd_pcm_format_t type)
    {
        uint32_t v;
        switch( type ) {
        case SND_PCM_FORMAT_S24_BE:
        case SND_PCM_FORMAT_S32_BE:
        case SND_PCM_FORMAT_S24_3BE:
        case SND_PCM_FORMAT_S20_3BE:
        case SND_PCM_FORMAT_S18_3BE:
            to_big_endian(value, &v);
            break;
        case SND_PCM_FORMAT_S24_LE:
        case SND_PCM_FORMAT_S32_LE:
        case SND_PCM_FORMAT_S24_3LE:
        case SND_PCM_FORMAT_S20_3LE:
        case SND_PCM_FORMAT_S18_3LE:
            to_little_endian(value, &v);
            break;
        default:
            assert(false);
        }

        switch( type ) {
        case SND_PCM_FORMAT_S24_LE:
        case SND_PCM_FORMAT_S24_BE:
        case SND_PCM_FORMAT_S32_LE:
        case SND_PCM_FORMAT_S32_BE:
            std::memcpy(d, &v, sizeof(v));
            break;
        case SND_PCM_FORMAT_S24_3LE:
        case SND_PCM_FORMAT_S20_3LE:
        case SND_PCM_FORMAT_S18_3LE:
            std::memcpy(d, &v, 3);
            break;
        case SND_PCM_FORMAT_S24_3BE:
        case SND_PCM_FORMAT_S20_3BE:
        case SND_PCM_FORMAT_S18_3BE:
            std::memcpy(d, (reinterpret_cast<char*>(&v) + 1), 3);
            break;
        default:
            assert(false);
        }
    }

    inline void pcm_buffer::channel_iter::write_f(float value, snd_pcm_format_t type)
    {
        switch( type ) {
        case SND_PCM_FORMAT_FLOAT_BE:
            to_big_endian(value, d);
            break;
        case SND_PCM_FORMAT_FLOAT_LE:
            to_little_endian(value, d);
            break;
        default:
            assert(false);
        }
    }

    template<>
    void pcm_buffer::channel_iter::write<int16_t>(int16_t value, snd_pcm_format_t type)
    {
        write_i16(value, type);
    }

    template<>
    void pcm_buffer::channel_iter::write<int32_t>(int32_t value, snd_pcm_format_t type)
    {
        write_i32(value, type);
    }

    template<>
    void pcm_buffer::channel_iter::write<float>(float value, snd_pcm_format_t type)
    {
        write_f(value, type);
    }

    inline pcm_device::pcm_device(const char* deviceName, snd_pcm_stream_t dir, int flags, int* error)
    {
        int err = snd_pcm_open(&m_handle, deviceName, dir, flags);
        if( error )
            *error = err;
    }

    inline int pcm_device::open(const char* deviceName, snd_pcm_stream_t dir, int flags)
    {
        cleanup();
        return snd_pcm_open(&m_handle, deviceName, dir, flags);
    }

    inline void pcm_device::close()
    {
        cleanup();
        m_handle = {};
    }

    inline pcm_buffer pcm_device::mmap_begin(snd_pcm_uframes_t frames, int* error)
    {
        pcm_buffer buf;
        buf.m_frames = frames;
        *error = snd_pcm_mmap_begin(m_handle, &buf.m_areas, &buf.m_offset, &buf.m_frames);
        return buf;
    }

    inline snd_pcm_sframes_t pcm_device::mmap_commit(const pcm_buffer& buffer)
    {
        return snd_pcm_mmap_commit(m_handle, buffer.offset(), buffer.frames());
    }

} // namespace alsa
} // namespace lbu

#endif // LIBLBU_ALSA_H

