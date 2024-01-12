/* Copyright 2015-2023 Zeno Sebastian Endemann <zeno.endemann@mailbox.org>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "lbu/alsa/alsa.h"

namespace lbu {
namespace alsa {

uint32_t hardware_params::set_best_matching_sample_rate(snd_pcm_t* pcm, snd_pcm_hw_params_t* hw_params,
                                                        uint32_t rate, alsa_error* error)
{
    unsigned min_rate = 0;
    unsigned max_rate = 0;
    int dir_min = 0;
    int dir_max = 0;
    alsa_error err;

    if( error )
        *error = 0;

    snd_pcm_hw_params_set_rate_resample(pcm, hw_params, 0); // ignore error - if the device cannot guarantee no resampling is happening, so be it

    if( err = snd_pcm_hw_params_set_rate(pcm, hw_params, rate, 0); err >= 0 )
        return rate;

    if( err = snd_pcm_hw_params_get_rate_min(hw_params, &min_rate, &dir_min); err < 0 )
        goto fail;
    if( err = snd_pcm_hw_params_get_rate_max(hw_params, &max_rate, &dir_max); err < 0 )
        goto fail;

    if( rate > max_rate ) {
        if( err = snd_pcm_hw_params_set_rate(pcm, hw_params, max_rate, dir_max); err >= 0 )
            return max_rate;
    } else if( rate < min_rate ) {
        if( err = snd_pcm_hw_params_set_rate(pcm, hw_params, min_rate, dir_min); err >= 0 )
            return min_rate;
    } else {
        if( err = snd_pcm_hw_params_set_rate(pcm, hw_params, rate, 1); err < 0 )
            goto fail;
        if( err = snd_pcm_hw_params_get_rate(hw_params, &rate, nullptr); err >= 0 )
            return rate;
    }

fail:
    if( error )
        *error = err;
    return 0;
}

snd_pcm_format_t hardware_params::set_best_signed_linear_pcm_format_type(snd_pcm_t* pcm, snd_pcm_hw_params_t* hw_params,
                                                                         snd_pcm_format_t type, alsa_error* error)
{
    alsa_error err;
    if( err = snd_pcm_hw_params_set_format(pcm, hw_params, type); err < 0 ) {
        pcm_format_type_mask type_mask;
        snd_pcm_hw_params_get_format_mask(hw_params, type_mask);
        type = pcm_format_type_mask::best_fallback_signed_linear_pcm_format_type(type_mask, type);
        if( type == SND_PCM_FORMAT_UNKNOWN )
            goto fail;
        if( err = snd_pcm_hw_params_set_format(pcm, hw_params, type); err < 0 )
            goto fail;
    }
    return type;

fail:
    if( error )
        *error = err;
    return SND_PCM_FORMAT_UNKNOWN;
}

snd_pcm_format_t pcm_format_type_mask::best_fallback_signed_linear_pcm_format_type(const snd_pcm_format_mask_t* mask, snd_pcm_format_t type)
{
    using format_array = array_ref<const snd_pcm_format_t>;
    using fallback_array = array_ref<const format_array>;

    static const snd_pcm_format_t S16a[] = {SND_PCM_FORMAT_S16_LE, SND_PCM_FORMAT_S16_BE};
    static const snd_pcm_format_t S18a[] = {SND_PCM_FORMAT_S18_3LE, SND_PCM_FORMAT_S18_3BE};
    static const snd_pcm_format_t S20a[] = {SND_PCM_FORMAT_S20_3LE, SND_PCM_FORMAT_S20_3BE};
    static const snd_pcm_format_t S24a[] = {SND_PCM_FORMAT_S24_LE, SND_PCM_FORMAT_S24_BE, SND_PCM_FORMAT_S24_3LE, SND_PCM_FORMAT_S24_3BE};
    static const snd_pcm_format_t S32a[] = {SND_PCM_FORMAT_S32_LE, SND_PCM_FORMAT_S32_BE};
    static const snd_pcm_format_t Fa[] = {SND_PCM_FORMAT_FLOAT_LE, SND_PCM_FORMAT_FLOAT_BE};
    static const snd_pcm_format_t F64a[] = {SND_PCM_FORMAT_FLOAT64_LE, SND_PCM_FORMAT_FLOAT64_BE};

    static const format_array S16(S16a);
    static const format_array S18(S18a);
    static const format_array S20(S20a);
    static const format_array S24(S24a);
    static const format_array S32(S32a);
    static const format_array Float(Fa);
    static const format_array F64(F64a);

    static const format_array S16Fallback[] = {S16, S24, S32, Float, F64, S20, S18};
    static const format_array S18Fallback[] = {S18, S20, S24, S32, Float, F64, S16};
    static const format_array S20Fallback[] = {S20, S24, S32, Float, F64, S18, S16};
    static const format_array S24Fallback[] = {S24, S32, Float, F64, S20, S18, S16};
    static const format_array S32Fallback[] = {S32, F64, Float, S24, S20, S18, S16};
    static const format_array FloatFallback[] = {Float, F64, S32, S24, S20, S18, S16};
    static const format_array F64Fallback[] = {F64, Float, S32, S24, S20, S18, S16};

    assert( ! snd_pcm_format_mask_test(mask, type) );

    fallback_array fallback;

    switch( type ) {
    case SND_PCM_FORMAT_S8:
    case SND_PCM_FORMAT_S16_LE:
    case SND_PCM_FORMAT_S16_BE:
        fallback = fallback_array(S16Fallback);
        break;
    case SND_PCM_FORMAT_S18_3LE:
    case SND_PCM_FORMAT_S18_3BE:
        fallback = fallback_array(S18Fallback);
        break;
    case SND_PCM_FORMAT_S20_3LE:
    case SND_PCM_FORMAT_S20_3BE:
        fallback = fallback_array(S20Fallback);
        break;
    case SND_PCM_FORMAT_S24_LE:
    case SND_PCM_FORMAT_S24_BE:
    case SND_PCM_FORMAT_S24_3LE:
    case SND_PCM_FORMAT_S24_3BE:
        fallback = fallback_array(S24Fallback);
        break;
    case SND_PCM_FORMAT_S32_LE:
    case SND_PCM_FORMAT_S32_BE:
        fallback = fallback_array(S32Fallback);
        break;
    case SND_PCM_FORMAT_FLOAT_LE:
    case SND_PCM_FORMAT_FLOAT_BE:
        fallback = fallback_array(FloatFallback);
        break;
    case SND_PCM_FORMAT_FLOAT64_LE:
    case SND_PCM_FORMAT_FLOAT64_BE:
        fallback = fallback_array(F64Fallback);
        break;
    default:
        assert(false); // not a signed pcm type
    }

    for( auto fa : fallback ) {
        for( auto f : fa ) {
            if( snd_pcm_format_mask_test(mask, f) )
                return f;
        }
    }
    return snd_pcm_format_mask_test(mask, SND_PCM_FORMAT_S8) ? SND_PCM_FORMAT_S8 : SND_PCM_FORMAT_UNKNOWN;
}

} // namespace alsa
} // namespace lbu
