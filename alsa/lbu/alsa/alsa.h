/* Copyright 2015-2023 Zeno Sebastian Endemann <zeno.endemann@mailbox.org>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LIBLBU_ALSA_H
#define LIBLBU_ALSA_H

#include "lbu/dynamic_memory.h"
#include "lbu/endian.h"

#include <alsa/asoundlib.h>
#include <cstring>

namespace lbu {
namespace alsa {

    using alsa_error = int;

    class card_list {
    public:
        class element_ptr {
        public:
            element_ptr();
            // default copy ctor/assignment ok

            bool is_valid() const { return idx >= 0; }
            explicit operator bool() const { return is_valid(); }
            alsa_error last_error() const { return err; }

            int index() const { return idx; }
            unique_cstr name();
            unique_cstr longname();

            element_ptr& operator++();

        private:
            int idx;
            alsa_error err;
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

        enum class Direction {
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

        explicit device_list(int card_index = AllCards, alsa_error* error = nullptr);
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


    struct pcm_format {
        snd_pcm_format_t type = SND_PCM_FORMAT_UNKNOWN;
        uint16_t channels = 0;
        uint32_t rate = 0;

        static bool is_linear_pcm_format_type(snd_pcm_format_t type);
        static bool is_signed_linear_pcm_format_type(snd_pcm_format_t type);

        static unsigned native_byte_size(snd_pcm_format_t linear_pcm_type);
        static unsigned packed_byte_size(snd_pcm_format_t linear_pcm_type);

        static unsigned frame_native_byte_size(snd_pcm_format_t linear_pcm_type, uint16_t channels);
        static unsigned frame_packed_byte_size(snd_pcm_format_t linear_pcm_type, uint16_t channels);
    };


    struct pcm_buffer {
        const snd_pcm_channel_area_t* areas = {};
        snd_pcm_uframes_t offset = 0;
        snd_pcm_uframes_t frames = 0;

        class channel_iter {
        public:
            channel_iter() = default;
            channel_iter(void* data, unsigned stride);
            // default copy ctor/assignment ok

            void* data() { return d; }
            const void* data() const { return d; }

            channel_iter& operator++();
            channel_iter& operator--();

            int16_t read_i16(snd_pcm_format_t type) const;
            int32_t read_i32(snd_pcm_format_t type) const;
            float read_f(snd_pcm_format_t type) const;
            double read_d(snd_pcm_format_t type) const;

            template< typename T >
            T read(snd_pcm_format_t type) const;

            void write_i16(int16_t value, snd_pcm_format_t type);
            void write_i32(int32_t value, snd_pcm_format_t type);
            void write_f(float value, snd_pcm_format_t type);
            void write_d(double value, snd_pcm_format_t type);

            template< typename T >
            void write(T value, snd_pcm_format_t type);

        private:
            void* d = {};
            unsigned stride = 0;
        };

        channel_iter begin(unsigned channel) const;
    };


    class pcm_device {
    public:
        enum Flags {
            FlagsNone = 0,
            FlagsNonBlock = SND_PCM_NONBLOCK,
            FlagsNoAutoResample = SND_PCM_NO_AUTO_RESAMPLE,
            FlagsNoAutoChannels = SND_PCM_NO_AUTO_CHANNELS,
            FlagsNoAutoFormat =  SND_PCM_NO_AUTO_FORMAT,
            FlagsNoAutoConversion = FlagsNoAutoResample | FlagsNoAutoChannels | FlagsNoAutoFormat
        };

        pcm_device() = default;
        pcm_device(const char* device_name, snd_pcm_stream_t dir,
                   int flags = FlagsNone, alsa_error* error = nullptr);

        pcm_device(const pcm_device&) = delete;
        pcm_device& operator=(const pcm_device&) = delete;

        ~pcm_device() { cleanup(); }

        alsa_error open(const char* device_name, snd_pcm_stream_t dir, int flags = FlagsNone);
        void close();

        bool is_valid() const { return device_handle; }
        explicit operator bool() const { return is_valid(); }

        snd_pcm_t* handle() const { return device_handle; }
        operator snd_pcm_t*() const { return handle(); }

        static pcm_buffer mmap_begin(snd_pcm_t* pcm, snd_pcm_uframes_t frames, alsa_error* error);
        static snd_pcm_sframes_t mmap_commit(snd_pcm_t* pcm, const pcm_buffer& buffer);

    private:
        void cleanup() { if( device_handle ) snd_pcm_close(device_handle); }

        snd_pcm_t* device_handle = {};
    };


    class hardware_params {
    public:
        hardware_params();
        ~hardware_params();

        hardware_params(const hardware_params& rhs);
        hardware_params& operator=(const hardware_params& rhs);

        snd_pcm_hw_params_t* handle() const { return hw_params; }
        operator snd_pcm_hw_params_t*() const { return handle(); }

        static uint32_t LIBLBU_EXPORT set_best_matching_sample_rate(snd_pcm_t* pcm, snd_pcm_hw_params_t* hw_params,
                                                                    uint32_t rate, alsa_error* error = nullptr);
        static snd_pcm_format_t LIBLBU_EXPORT set_best_signed_linear_pcm_format_type(snd_pcm_t* pcm, snd_pcm_hw_params_t* hw_params,
                                                                                     snd_pcm_format_t type, alsa_error* error = nullptr);

    private:
        snd_pcm_hw_params_t* hw_params = {};
    };


    class software_params {
    public:
        software_params();
        ~software_params();

        software_params(const software_params& rhs);
        software_params& operator=(const software_params& rhs);

        snd_pcm_sw_params_t* handle() const { return sw_params; }
        operator snd_pcm_sw_params_t*() const { return handle(); }

    private:
        snd_pcm_sw_params_t* sw_params = {};
    };


    class pcm_format_type_mask {
    public:
        pcm_format_type_mask();
        ~pcm_format_type_mask();

        pcm_format_type_mask(const pcm_format_type_mask& rhs);
        pcm_format_type_mask& operator=(const pcm_format_type_mask& rhs);

        snd_pcm_format_mask_t* handle() const { return mask; }
        operator snd_pcm_format_mask_t*() const { return handle(); }

        static snd_pcm_format_t LIBLBU_EXPORT best_fallback_signed_linear_pcm_format_type(const snd_pcm_format_mask_t* mask, snd_pcm_format_t type);

    private:
        snd_pcm_format_mask_t* mask = {};
    };


    class pcm_device_status {
    public:
        pcm_device_status();
        ~pcm_device_status();

        pcm_device_status(const pcm_device_status&) = delete;
        pcm_device_status& operator=(const pcm_device_status&) = delete;

        snd_pcm_status_t* handle() const { return status; }
        operator snd_pcm_status_t*() const { return handle(); }

    private:
        snd_pcm_status_t* status = {};
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

    inline device_list::device_list(int card_index, alsa_error* error)
    {
        alsa_error err = snd_device_name_hint(card_index, "pcm", &hints);
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

    inline bool pcm_format::is_linear_pcm_format_type(snd_pcm_format_t type)
    {
        switch( type ) {
        case SND_PCM_FORMAT_S8:
        case SND_PCM_FORMAT_U8:
        case SND_PCM_FORMAT_S16_LE:
        case SND_PCM_FORMAT_S16_BE:
        case SND_PCM_FORMAT_U16_LE:
        case SND_PCM_FORMAT_U16_BE:
        case SND_PCM_FORMAT_S20_LE:
        case SND_PCM_FORMAT_S20_BE:
        case SND_PCM_FORMAT_U20_LE:
        case SND_PCM_FORMAT_U20_BE:
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
        case SND_PCM_FORMAT_FLOAT64_LE:
        case SND_PCM_FORMAT_FLOAT64_BE:
            return true;
        default:
            return false;
        }
    }

    inline bool pcm_format::is_signed_linear_pcm_format_type(snd_pcm_format_t type)
    {
        switch( type ) {
        case SND_PCM_FORMAT_S8:
        case SND_PCM_FORMAT_S16_LE:
        case SND_PCM_FORMAT_S16_BE:
        case SND_PCM_FORMAT_S18_3LE:
        case SND_PCM_FORMAT_S18_3BE:
        case SND_PCM_FORMAT_S20_LE:
        case SND_PCM_FORMAT_S20_BE:
        case SND_PCM_FORMAT_S20_3LE:
        case SND_PCM_FORMAT_S20_3BE:
        case SND_PCM_FORMAT_S24_LE:
        case SND_PCM_FORMAT_S24_BE:
        case SND_PCM_FORMAT_S24_3LE:
        case SND_PCM_FORMAT_S24_3BE:
        case SND_PCM_FORMAT_S32_LE:
        case SND_PCM_FORMAT_S32_BE:
        case SND_PCM_FORMAT_FLOAT_LE:
        case SND_PCM_FORMAT_FLOAT_BE:
        case SND_PCM_FORMAT_FLOAT64_LE:
        case SND_PCM_FORMAT_FLOAT64_BE:
            return true;
        default:
            return false;
        }
    }

    inline unsigned pcm_format::native_byte_size(snd_pcm_format_t linear_pcm_type)
    {
        switch( linear_pcm_type ) {
        case SND_PCM_FORMAT_S8:
        case SND_PCM_FORMAT_U8:
            return 1;
        case SND_PCM_FORMAT_S16_LE:
        case SND_PCM_FORMAT_S16_BE:
        case SND_PCM_FORMAT_U16_LE:
        case SND_PCM_FORMAT_U16_BE:
            return 2;
        case SND_PCM_FORMAT_S20_LE:
        case SND_PCM_FORMAT_S20_BE:
        case SND_PCM_FORMAT_U20_LE:
        case SND_PCM_FORMAT_U20_BE:
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
        case SND_PCM_FORMAT_S18_3LE:
        case SND_PCM_FORMAT_S18_3BE:
        case SND_PCM_FORMAT_U18_3LE:
        case SND_PCM_FORMAT_U18_3BE:
        case SND_PCM_FORMAT_S20_3LE:
        case SND_PCM_FORMAT_S20_3BE:
        case SND_PCM_FORMAT_U20_3LE:
        case SND_PCM_FORMAT_U20_3BE:
        case SND_PCM_FORMAT_S24_3LE:
        case SND_PCM_FORMAT_S24_3BE:
        case SND_PCM_FORMAT_U24_3LE:
        case SND_PCM_FORMAT_U24_3BE:
            return 4;
        case SND_PCM_FORMAT_FLOAT64_LE:
        case SND_PCM_FORMAT_FLOAT64_BE:
            return 8;
        default:
            assert(false);
            return 0;
        }
    }

    inline unsigned pcm_format::packed_byte_size(snd_pcm_format_t linear_pcm_type)
    {
        switch( linear_pcm_type ) {
        case SND_PCM_FORMAT_S8:
        case SND_PCM_FORMAT_U8:
            return 1;
        case SND_PCM_FORMAT_S16_LE:
        case SND_PCM_FORMAT_S16_BE:
        case SND_PCM_FORMAT_U16_LE:
        case SND_PCM_FORMAT_U16_BE:
            return 2;
        case SND_PCM_FORMAT_S20_LE:
        case SND_PCM_FORMAT_S20_BE:
        case SND_PCM_FORMAT_U20_LE:
        case SND_PCM_FORMAT_U20_BE:
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
        case SND_PCM_FORMAT_S18_3LE:
        case SND_PCM_FORMAT_S18_3BE:
        case SND_PCM_FORMAT_U18_3LE:
        case SND_PCM_FORMAT_U18_3BE:
        case SND_PCM_FORMAT_S20_3LE:
        case SND_PCM_FORMAT_S20_3BE:
        case SND_PCM_FORMAT_U20_3LE:
        case SND_PCM_FORMAT_U20_3BE:
        case SND_PCM_FORMAT_S24_3LE:
        case SND_PCM_FORMAT_S24_3BE:
        case SND_PCM_FORMAT_U24_3LE:
        case SND_PCM_FORMAT_U24_3BE:
            return 3;
        default:
            assert(false);
            return 0;
        }
    }

    inline unsigned int pcm_format::frame_native_byte_size(snd_pcm_format_t linear_pcm_type, uint16_t channels)
    {
        return native_byte_size(linear_pcm_type) * channels;
    }

    inline unsigned int pcm_format::frame_packed_byte_size(snd_pcm_format_t linear_pcm_type, uint16_t channels)
    {
        return packed_byte_size(linear_pcm_type) * channels;
    }

    inline pcm_buffer::channel_iter pcm_buffer::begin(unsigned channel) const
    {
        assert((areas[channel].first & 7) == 0);
        assert((areas[channel].step & 7) == 0);

        const unsigned stride = (areas[channel].step >> 3);
        char* addr = static_cast<char*>(areas[channel].addr);
        addr += (areas[channel].first >> 3);
        addr += stride * offset;
        return {addr, stride};
    }

    inline pcm_buffer::channel_iter::channel_iter(void* data, unsigned stride)
        : d(data), stride(stride)
    {}

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
        switch( type ) {
        case SND_PCM_FORMAT_S16_BE:
            return from_big_endian<int16_t>(d);
        case SND_PCM_FORMAT_S16_LE:
            return from_little_endian<int16_t>(d);
        default:
            assert(false);
            return 0;
        }
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
        case SND_PCM_FORMAT_S20_LE:
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
        case SND_PCM_FORMAT_S20_BE: {
            auto v = from_big_endian_u24_packed(static_cast<const char*>(d) + 1);
            if( v >= (uint32_t(1) << 19) )
                v |= uint32_t(0xfff) << 20;
            return value_reinterpret_cast<uint32_t, int32_t>(v);
        }
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

    inline double pcm_buffer::channel_iter::read_d(snd_pcm_format_t type) const
    {
        switch( type ) {
        case SND_PCM_FORMAT_FLOAT64_BE:
            return from_big_endian<double>(d);
        case SND_PCM_FORMAT_FLOAT64_LE:
            return from_little_endian<double>(d);
        default:
            assert(false);
            return 0;
        }
    }

    template<>
    inline int16_t pcm_buffer::channel_iter::read<int16_t>(snd_pcm_format_t type) const
    {
        return read_i16(type);
    }

    template<>
    inline int32_t pcm_buffer::channel_iter::read<int32_t>(snd_pcm_format_t type) const
    {
        return read_i32(type);
    }

    template<>
    inline float pcm_buffer::channel_iter::read<float>(snd_pcm_format_t type) const
    {
        return read_f(type);
    }

    template<>
    inline double pcm_buffer::channel_iter::read<double>(snd_pcm_format_t type) const
    {
        return read_d(type);
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
        char v[4];
        switch( type ) {
        case SND_PCM_FORMAT_S16_BE:
        case SND_PCM_FORMAT_S16_LE:
            write_i16(value, type);
            return;
        case SND_PCM_FORMAT_S18_3BE:
        case SND_PCM_FORMAT_S20_BE:
        case SND_PCM_FORMAT_S20_3BE:
        case SND_PCM_FORMAT_S24_BE:
        case SND_PCM_FORMAT_S24_3BE:
        case SND_PCM_FORMAT_S32_BE:
            to_big_endian(value, v);
            break;
        case SND_PCM_FORMAT_S18_3LE:
        case SND_PCM_FORMAT_S20_LE:
        case SND_PCM_FORMAT_S20_3LE:
        case SND_PCM_FORMAT_S24_LE:
        case SND_PCM_FORMAT_S24_3LE:
        case SND_PCM_FORMAT_S32_LE:
            to_little_endian(value, v);
            break;
        default:
            assert(false);
        }

        switch( type ) {
        case SND_PCM_FORMAT_S20_LE:
        case SND_PCM_FORMAT_S20_BE:
        case SND_PCM_FORMAT_S24_LE:
        case SND_PCM_FORMAT_S24_BE:
        case SND_PCM_FORMAT_S32_LE:
        case SND_PCM_FORMAT_S32_BE:
            std::memcpy(d, v, 4);
            break;
        case SND_PCM_FORMAT_S24_3LE:
        case SND_PCM_FORMAT_S20_3LE:
        case SND_PCM_FORMAT_S18_3LE:
            std::memcpy(d, v, 3);
            break;
        case SND_PCM_FORMAT_S24_3BE:
        case SND_PCM_FORMAT_S20_3BE:
        case SND_PCM_FORMAT_S18_3BE:
            std::memcpy(d, (v + 1), 3);
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

    inline void pcm_buffer::channel_iter::write_d(double value, snd_pcm_format_t type)
    {
        switch( type ) {
        case SND_PCM_FORMAT_FLOAT64_BE:
            to_big_endian(value, d);
            break;
        case SND_PCM_FORMAT_FLOAT64_LE:
            to_little_endian(value, d);
            break;
        default:
            assert(false);
        }
    }

    template<>
    inline void pcm_buffer::channel_iter::write<int16_t>(int16_t value, snd_pcm_format_t type)
    {
        write_i16(value, type);
    }

    template<>
    inline void pcm_buffer::channel_iter::write<int32_t>(int32_t value, snd_pcm_format_t type)
    {
        write_i32(value, type);
    }

    template<>
    inline void pcm_buffer::channel_iter::write<float>(float value, snd_pcm_format_t type)
    {
        write_f(value, type);
    }

    template<>
    inline void pcm_buffer::channel_iter::write<double>(double value, snd_pcm_format_t type)
    {
        write_d(value, type);
    }

    inline pcm_device::pcm_device(const char* device_name, snd_pcm_stream_t dir, int flags, alsa_error* error)
    {
        alsa_error err = snd_pcm_open(&device_handle, device_name, dir, flags);
        if( error )
            *error = err;
    }

    inline alsa_error pcm_device::open(const char* device_name, snd_pcm_stream_t dir, int flags)
    {
        cleanup();
        return snd_pcm_open(&device_handle, device_name, dir, flags);
    }

    inline void pcm_device::close()
    {
        cleanup();
        device_handle = {};
    }

    inline pcm_buffer pcm_device::mmap_begin(snd_pcm_t* pcm, snd_pcm_uframes_t frames, alsa_error* error)
    {
        pcm_buffer buf;
        buf.frames = frames;
        *error = snd_pcm_mmap_begin(pcm, &buf.areas, &buf.offset, &buf.frames);
        return buf;
    }

    inline snd_pcm_sframes_t pcm_device::mmap_commit(snd_pcm_t* pcm, const pcm_buffer& buffer)
    {
        return snd_pcm_mmap_commit(pcm, buffer.offset, buffer.frames);
    }

    inline hardware_params::hardware_params()
    {
        if( snd_pcm_hw_params_malloc(&hw_params) < 0 )
            unexpected_memory_exhaustion();
    }

    inline hardware_params::~hardware_params()
    {
        snd_pcm_hw_params_free(hw_params);
    }

    inline hardware_params::hardware_params(const hardware_params& rhs)
    {
        if( snd_pcm_hw_params_malloc(&hw_params) < 0 )
            unexpected_memory_exhaustion();
        snd_pcm_hw_params_copy(hw_params, rhs.hw_params);
    }

    inline hardware_params& hardware_params::operator=(const hardware_params& rhs)
    {
        snd_pcm_hw_params_copy(hw_params, rhs.hw_params);
        return *this;
    }

    inline software_params::software_params()
    {
        if( snd_pcm_sw_params_malloc(&sw_params) < 0 )
            unexpected_memory_exhaustion();
    }

    inline software_params::~software_params()
    {
        snd_pcm_sw_params_free(sw_params);
    }

    inline software_params::software_params(const software_params& rhs)
    {
        if( snd_pcm_sw_params_malloc(&sw_params) < 0 )
            unexpected_memory_exhaustion();
        snd_pcm_sw_params_copy(sw_params, rhs.sw_params);
    }

    inline software_params& software_params::operator=(const software_params& rhs)
    {
        snd_pcm_sw_params_copy(sw_params, rhs.sw_params);
        return *this;
    }

    inline pcm_format_type_mask::pcm_format_type_mask()
    {
        if( snd_pcm_format_mask_malloc(&mask) < 0 )
            unexpected_memory_exhaustion();
    }

    inline pcm_format_type_mask::~pcm_format_type_mask()
    {
        snd_pcm_format_mask_free(mask);
    }

    inline pcm_format_type_mask::pcm_format_type_mask(const pcm_format_type_mask& rhs)
    {
        if( snd_pcm_format_mask_malloc(&mask) < 0 )
            unexpected_memory_exhaustion();
        snd_pcm_format_mask_copy(mask, rhs.mask);
    }

    inline pcm_format_type_mask& pcm_format_type_mask::operator=(const pcm_format_type_mask& rhs)
    {
        snd_pcm_format_mask_copy(mask, rhs.mask);
        return *this;
    }

    inline pcm_device_status::pcm_device_status()
    {
        if( snd_pcm_status_malloc(&status) < 0 )
            unexpected_memory_exhaustion();
    }

    inline pcm_device_status::~pcm_device_status()
    {
        snd_pcm_status_free(status);
    }
}
}

#endif
