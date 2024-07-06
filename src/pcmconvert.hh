/***
exocaster -- audio streaming helper
pcmconvert.hh -- PCM conversion

MIT License 

Copyright (c) 2024 ziplantil

Permission is hereby granted, free of charge, to any person obtaining a 
copy of this software and associated documentation files (the "Software"), 
to deal in the Software without restriction, including without limitation 
the rights to use, copy, modify, merge, publish, distribute, sublicense, 
and/or sell copies of the Software, and to permit persons to whom the 
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in 
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS 
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
DEALINGS IN THE SOFTWARE.

***/

#ifndef PCMCONVERT_HH
#define PCMCONVERT_HH

#include <algorithm>
#include <cmath>
#include <limits>
#include <cstdlib>
#include <cstring>
#include <type_traits>

#include "pcmtypes.hh"
#include "types.hh"
#include "unaligned.hh"
#include "util.hh"

namespace exo {

template <typename TOut, typename TIn>
TOut clampInt_(TIn v) {
    if (v < std::numeric_limits<TOut>::min())
        v = std::numeric_limits<TOut>::min();
    if (v > std::numeric_limits<TOut>::max())
        v = std::numeric_limits<TOut>::max();
    return static_cast<TOut>(v);
}

template <bool round, typename TInt, typename TFloat>
TInt floatToInt_(TFloat x) {
    if constexpr (round)
        return static_cast<TInt>(std::round(x));
    else
        return static_cast<TInt>(std::floor(x));
}

template <exo::PcmSampleFormat fmt>
requires (exo::IsSampleUnsignedInt_v<fmt> || exo::IsSampleSignedInt_v<fmt>)
constexpr exo::WiderType_t<exo::PcmFormat_t<fmt>> getSampleFormatBias_() {
    using W = exo::WiderType_t<exo::PcmFormat_t<fmt>>;
    constexpr auto min = static_cast<W>(exo::PcmFormatDefs<fmt>::min);
    constexpr auto max = static_cast<W>(exo::PcmFormatDefs<fmt>::max);
    return (max - min + 1) / 2;
}

template <exo::PcmSampleFormat fmt, bool round, std::floating_point F>
inline exo::PcmFormat_t<fmt> convertSampleFromFloat_(const F& x,
                                                     const F noise = 0) {
    using T = exo::PcmFormat_t<fmt>;
    if constexpr (exo::IsSampleFloatingPoint_v<fmt>) {
        return static_cast<T>(std::clamp(static_cast<T>(x), T{-1}, T{+1}));
    } else if constexpr (std::is_unsigned_v<T>) {
        using W = exo::WiderType_t<T>;
        constexpr auto min = static_cast<W>(exo::PcmFormatDefs<fmt>::min);
        constexpr auto max = static_cast<W>(exo::PcmFormatDefs<fmt>::max);
        static_assert(min == 0);

        constexpr auto bias = exo::getSampleFormatBias_<fmt>();
        constexpr auto scale = std::max(max - bias, bias - min);
        return exo::clampInt_<T>(exo::floatToInt_<round, W>(
                    x * scale + bias + noise));

    } else {
        using W = exo::WiderType_t<T>;
        constexpr auto min = static_cast<W>(exo::PcmFormatDefs<fmt>::min);
        constexpr auto max = static_cast<W>(exo::PcmFormatDefs<fmt>::max);
        constexpr auto scale = std::max(-min, max);

        return exo::clampInt_<T>(exo::floatToInt_<round, W>(
                    x * scale + noise));
    }
}

template <typename T>
exo::byte* outputSample_(exo::byte* ptr, T value) {
    exo::unalignedStore<T>(ptr, value);
    return ptr + sizeof(T);
}

template <bool round = true, std::floating_point F>
inline exo::byte* outputSample(exo::byte* ptr, exo::PcmSampleFormat fmt, F d) {
    d = std::clamp(d, F{-1}, F{+1});
    switch (fmt) {
#define EXO_PCM_FORMATS_CASE(F)                                                \
    case exo::PcmSampleFormat::F:                                              \
        return exo::outputSample_(ptr,                                         \
                   exo::convertSampleFromFloat_<exo::PcmSampleFormat::F,       \
                            round>(d));
    EXO_PCM_FORMATS_SWITCH
#undef EXO_PCM_FORMATS_CASE
        default: EXO_UNREACHABLE;
    }
}

template <exo::PcmSampleFormat fmt, std::floating_point F>
inline F convertSampleToFloat(const exo::PcmFormat_t<fmt>& x) {
    using T = exo::PcmFormat_t<fmt>;
    F y;
    if constexpr (exo::IsSampleFloatingPoint_v<fmt>) {
        y = x;
    } else if constexpr (std::is_unsigned_v<T>) {
        using W = exo::WiderType_t<T>;
        constexpr auto min = static_cast<W>(exo::PcmFormatDefs<fmt>::min);
        constexpr auto max = static_cast<W>(exo::PcmFormatDefs<fmt>::max);
        static_assert(min == 0);

        constexpr auto bias = exo::getSampleFormatBias_<fmt>();
        constexpr auto scale = F{1.0} / static_cast<F>(
                                std::max(max - bias, bias - min));
        y = (x - bias) * scale;

    } else {
        using W = exo::WiderType_t<T>;
        constexpr auto min = static_cast<W>(exo::PcmFormatDefs<fmt>::min);
        constexpr auto max = static_cast<W>(exo::PcmFormatDefs<fmt>::max);
        constexpr auto scale = F{1.0} / static_cast<F>(std::max(-min, max));
        y = x * scale;
    }

    return static_cast<F>(std::clamp(static_cast<F>(y), F{-1}, F{+1}));
}

template <typename TA, typename TB>
requires (std::is_integral_v<TA> && std::is_integral_v<TB>
         && std::is_signed_v<TA> == std::is_signed_v<TB>)
// true if TA fits in TB, otherwise false
struct FitsIn {
    static constexpr bool value =
            std::numeric_limits<TB>::min() <= std::numeric_limits<TA>::min()
         && std::numeric_limits<TA>::max() <= std::numeric_limits<TB>::max();
};

template <typename TA, typename TB>
requires (std::is_integral_v<TA> && std::is_integral_v<TB>
         && std::is_signed_v<TA> == std::is_signed_v<TB>)
// wider of the two integer types TA and TB
struct Wider {
    using type = std::conditional_t<exo::FitsIn<TA, TB>::value &&
                                   !exo::FitsIn<TB, TA>::value, TB, TA>;
};

template <typename TA, typename TB>
using Wider_t = exo::Wider<TA, TB>::type;

template <std::uintmax_t newMax, std::uintmax_t oldMax>
struct ScaleUnsignedSample_ {
    using W = std::uintmax_t;
    static_assert(oldMax + 1 != 0);
    static_assert((oldMax + 1) * (oldMax + 1) > oldMax);
    inline W operator()(W x) {
        return (x * (oldMax + 1)) / (newMax + 1);
    }
};

template <>
struct ScaleUnsignedSample_<UINT8_MAX, UINT16_MAX> {
    inline auto operator()(std::uintmax_t x) { return x >> 8; }
};

template <>
struct ScaleUnsignedSample_<UINT16_MAX, UINT8_MAX> {
    inline auto operator()(std::uintmax_t x) { return (x << 8) | x; }
};

template <>
struct ScaleUnsignedSample_<UINT16_MAX, UINT32_MAX> {
    inline auto operator()(std::uintmax_t x) { return x >> 16; }
};

template <>
struct ScaleUnsignedSample_<UINT32_MAX, UINT16_MAX> {
    inline auto operator()(std::uintmax_t x) { return (x << 16) | x; }
};

template <>
struct ScaleUnsignedSample_<INT8_MAX, UINT32_MAX> {
    inline auto operator()(std::uintmax_t x) { return x >> 24; }
};

template <>
struct ScaleUnsignedSample_<UINT32_MAX, UINT8_MAX> {
    inline auto operator()(std::uintmax_t x) {
        return (x << 24) | (x << 16) | (x << 8) | x;
    }
};

template <exo::PcmSampleFormat fdst, exo::PcmSampleFormat fsrc>
requires ((exo::IsSampleSignedInt_v<fdst> || exo::IsSampleUnsignedInt_v<fdst>)
       && (exo::IsSampleSignedInt_v<fsrc> || exo::IsSampleUnsignedInt_v<fsrc>))
inline exo::PcmFormat_t<fdst> convertSampleIntToInt_(
                            exo::PcmFormat_t<fsrc> x) {
    using Tdst = exo::PcmFormat_t<fdst>;
    using Tsrc = exo::PcmFormat_t<fsrc>;
    using S = exo::Wider_t<exo::WiderType_t<Tdst>, exo::WiderType_t<Tsrc>>;
    using U = std::make_unsigned_t<S>;

    U u1;
    if constexpr (exo::IsSampleSignedInt_v<fsrc>) {
        S s1 = static_cast<S>(x);
        s1 -= exo::PcmFormatDefs<fsrc>::min;
        u1 = static_cast<U>(s1);
    } else {
        u1 = static_cast<U>(x);
    }

    constexpr U newMax = static_cast<U>(
                  static_cast<S>(exo::PcmFormatDefs<fdst>::max)
                - static_cast<S>(exo::PcmFormatDefs<fdst>::min));
    constexpr U oldMax = static_cast<U>(
                  static_cast<S>(exo::PcmFormatDefs<fsrc>::max)
                - static_cast<S>(exo::PcmFormatDefs<fsrc>::min));
    struct exo::ScaleUnsignedSample_<newMax, oldMax> conv;
    U u2 = static_cast<U>(conv(u1));

    if constexpr (exo::IsSampleSignedInt_v<fdst>) {
        S s2 = static_cast<S>(u2);
        s2 += exo::PcmFormatDefs<fdst>::min;
        return static_cast<Tdst>(s2);
    } else {
        return static_cast<Tdst>(u2);
    }
}

} // namespace exo

#endif /* PCMCONVERT_HH */

