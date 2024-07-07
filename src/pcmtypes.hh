/***
exocaster -- audio streaming helper
pcmtypes.hh -- PCM types

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

#ifndef PCMTYPES_HH
#define PCMTYPES_HH

#include <algorithm>
#include <climits>
#include <concepts>
#include <cstdint>
#include <limits>
#include <string>
#include <type_traits>

#include "types.hh"
#include "util.hh"

namespace exo {

static_assert(CHAR_BIT == 8);

#define EXO_PCM_FORMATS_SWITCH                                                 \
    EXO_PCM_FORMATS_CASE(S8)                                                   \
    EXO_PCM_FORMATS_CASE(U8)                                                   \
    EXO_PCM_FORMATS_CASE(S16)                                                  \
    EXO_PCM_FORMATS_CASE(F32)

enum class PcmSampleFormat {
    // all formats use native endianness if multi-byte
#define EXO_PCM_FORMATS_CASE(T) T,
    EXO_PCM_FORMATS_SWITCH
#undef EXO_PCM_FORMATS_CASE
};

template <typename T>
struct WiderType { };

template <>
struct WiderType<std::int8_t> { using type = std::int_fast16_t; };

template <>
struct WiderType<std::int16_t> { using type = std::int_fast32_t; };

template <>
struct WiderType<std::int32_t> { using type = std::int_fast64_t; };

template <>
struct WiderType<std::uint8_t> { using type = std::int_fast16_t; };

template <>
struct WiderType<std::uint16_t> { using type = std::int_fast32_t; };

template <>
struct WiderType<std::uint32_t> { using type = std::int_fast64_t; };

template <typename T>
requires (std::floating_point<T>)
struct WiderType<T> { using type = T; };

template <typename T>
using WiderType_t = WiderType<T>::type;

template <exo::PcmSampleFormat fmt>
struct PcmFormatDefs { };

template <std::integral T>
struct PcmFormatDefsFixed_ {
    using type = T;
    static constexpr type min = std::numeric_limits<type>::min();
    static constexpr type max = std::numeric_limits<type>::max();
};

template <std::floating_point T>
struct PcmFormatDefsFloat_ {
    using type = T;
    static constexpr type min = static_cast<type>(-1);
    static constexpr type max = static_cast<type>(+1);
};

template <>
struct PcmFormatDefs<PcmSampleFormat::S8>:
        public exo::PcmFormatDefsFixed_<std::int8_t> {
};

template <>
struct PcmFormatDefs<PcmSampleFormat::U8>:
        public exo::PcmFormatDefsFixed_<std::uint8_t> {
};

template <>
struct PcmFormatDefs<PcmSampleFormat::S16>:
        public exo::PcmFormatDefsFixed_<std::int16_t> {
};

template <>
struct PcmFormatDefs<PcmSampleFormat::F32>:
        public exo::PcmFormatDefsFloat_<float> {
};

template <exo::PcmSampleFormat fmt>
using PcmFormat_t = typename PcmFormatDefs<fmt>::type;

inline constexpr std::size_t bytesPerSampleFormat(exo::PcmSampleFormat fmt) {
    switch (fmt) {
#define EXO_PCM_FORMATS_CASE(F)                                                \
    case exo::PcmSampleFormat::F:                                              \
        return sizeof(exo::PcmFormat_t<exo::PcmSampleFormat::F>);
    EXO_PCM_FORMATS_SWITCH
#undef EXO_PCM_FORMATS_CASE
        default: EXO_UNREACHABLE;
    }
}

template <exo::PcmSampleFormat fmt>
constexpr bool IsSampleSignedInt_v =
        std::is_integral_v<exo::PcmFormat_t<fmt>> &&
        std::is_signed_v<exo::PcmFormat_t<fmt>>;

template <exo::PcmSampleFormat fmt>
constexpr bool IsSampleUnsignedInt_v =
        std::is_integral_v<exo::PcmFormat_t<fmt>> &&
        std::is_unsigned_v<exo::PcmFormat_t<fmt>>;

template <exo::PcmSampleFormat fmt>
constexpr bool IsSampleFloatingPoint_v =
        std::is_floating_point_v<exo::PcmFormat_t<fmt>>;

inline constexpr bool areSamplesSignedInt(exo::PcmSampleFormat fmt) {
    switch (fmt) {
#define EXO_PCM_FORMATS_CASE(F)                                                \
    case exo::PcmSampleFormat::F:                                              \
        return exo::IsSampleSignedInt_v<exo::PcmSampleFormat::F>;
    EXO_PCM_FORMATS_SWITCH
#undef EXO_PCM_FORMATS_CASE
        default: EXO_UNREACHABLE;
    }
}

inline constexpr bool areSamplesUnsignedInt(exo::PcmSampleFormat fmt) {
    switch (fmt) {
#define EXO_PCM_FORMATS_CASE(F)                                                \
    case exo::PcmSampleFormat::F:                                              \
        return exo::IsSampleUnsignedInt_v<exo::PcmSampleFormat::F>;
    EXO_PCM_FORMATS_SWITCH
#undef EXO_PCM_FORMATS_CASE
        default: EXO_UNREACHABLE;
    }
}

inline constexpr bool areSamplesFloatingPoint(exo::PcmSampleFormat fmt) {
    switch (fmt) {
#define EXO_PCM_FORMATS_CASE(F)                                                \
    case exo::PcmSampleFormat::F:                                              \
        return exo::IsSampleFloatingPoint_v<exo::PcmSampleFormat::F>;
    EXO_PCM_FORMATS_SWITCH
#undef EXO_PCM_FORMATS_CASE
        default: EXO_UNREACHABLE;
    }
}

#define EXO_PCM_FORMATS_CASE(F)                                                \
        sizeof(exo::PcmFormat_t<exo::PcmSampleFormat::F>),
inline constexpr std::size_t MAX_BYTES_PER_SAMPLE = std::max({
    EXO_PCM_FORMATS_SWITCH
});
#undef EXO_PCM_FORMATS_CASE

#define EXO_PCM_CHANNELS_SWITCH                                                \
    EXO_PCM_CHANNELS_CASE(Mono)                                                \
    EXO_PCM_CHANNELS_CASE(Stereo)

enum class PcmChannelLayout {
#define EXO_PCM_CHANNELS_CASE(C) C,
    EXO_PCM_CHANNELS_SWITCH
#undef EXO_PCM_CHANNELS_CASE
};

template <exo::PcmChannelLayout C>
struct PcmChannelDefs { };

template <>
struct PcmChannelDefs<exo::PcmChannelLayout::Mono> {
    static constexpr unsigned channels = 1;
};

template <>
struct PcmChannelDefs<exo::PcmChannelLayout::Stereo> {
    static constexpr unsigned channels = 2;
};

inline constexpr std::size_t MAX_CHANNELS = 8;

template <exo::PcmChannelLayout C>
inline constexpr unsigned channelCount_() {
    constexpr unsigned channels = exo::PcmChannelDefs<C>::channels;
    static_assert(channels <= MAX_CHANNELS);
    return channels;
}

inline constexpr unsigned channelCount(exo::PcmChannelLayout layout) {
    switch (layout) {
#define EXO_PCM_CHANNELS_CASE(C)                                               \
    case exo::PcmChannelLayout::C:                                             \
        return exo::channelCount_<exo::PcmChannelLayout::C>();
    EXO_PCM_CHANNELS_SWITCH
#undef EXO_PCM_CHANNELS_CASE
        default: EXO_UNREACHABLE;
    }
}

inline constexpr std::size_t MAX_BYTES_PER_FRAME =
                MAX_BYTES_PER_SAMPLE * MAX_CHANNELS;

struct PcmFormat {
    exo::PcmSampleFormat sample;
    unsigned long rate;
    exo::PcmChannelLayout channels;

    inline std::size_t durationToFrameCount(double duration) const noexcept {
        return static_cast<std::size_t>(duration * rate);
    }

    inline constexpr std::size_t bytesPerSample() const noexcept {
        return exo::bytesPerSampleFormat(sample);
    }

    inline constexpr std::size_t bytesPerFrame() const noexcept {
        return bytesPerSample() * exo::channelCount(channels);
    }

    inline double estimateDuration(std::size_t bytes) const noexcept {
        bytes /= bytesPerFrame();
        return static_cast<double>(bytes) / rate;
    }
};

} // namespace exo

#endif /* PCMTYPES_HH */

