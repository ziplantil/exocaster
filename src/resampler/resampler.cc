/***
exocaster -- audio streaming helper
resampler/resampler.cc -- multi-channel resampler

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
#include <exception>
#include <iostream>
#include <unordered_map>

#include "log.hh"
#include "pcmtypes.hh"
#include "resampler/resampler.hh"

#if EXO_SOXR
#include "resampler/soxr/soxr.hh"
#define RESAMPLER_DEFS_SOXR RESAMPLER_DEF(soxr, exo::SoxResampler)
#else
#define RESAMPLER_DEFS_SOXR
#endif

#if EXO_LIBSAMPLERATE
#include "resampler/libsamplerate/src.hh"
#define RESAMPLER_DEFS_LIBSAMPLERATE                                           \
    RESAMPLER_DEF(libsamplerate, exo::SrcResampler)
#else
#define RESAMPLER_DEFS_LIBSAMPLERATE
#endif

#if EXO_LIBAVCODEC
#include "resampler/libswresample/lswr.hh"
#define RESAMPLER_DEFS_LIBSWRESAMPLE                                           \
    RESAMPLER_DEF(libswresample, exo::LswrResampler)
#else
#define RESAMPLER_DEFS_LIBSWRESAMPLE
#endif

namespace exo {

#define RESAMPLER_DEFS                                                         \
    RESAMPLER_DEFS_SOXR                                                        \
    RESAMPLER_DEFS_LIBSAMPLERATE                                               \
    RESAMPLER_DEFS_LIBSWRESAMPLE

enum class ResamplerImpl {
    none, // otherwise the enum may be empty
#define RESAMPLER_DEF(name, T) name,
    RESAMPLER_DEFS
#undef RESAMPLER_DEF
};

static std::unordered_map<std::string, exo::ResamplerImpl> resamplers = {
#define RESAMPLER_DEF(N, T) {#N, exo::ResamplerImpl::N},
    RESAMPLER_DEFS
#undef RESAMPLER_DEF
};

std::unique_ptr<exo::BaseMultiChannelResampler>
createResampler(const std::string& type, const exo::ConfigObject& config,
                exo::PcmFormat sourcePcmFormat, exo::SampleRate targetRate) {
    [[maybe_unused]] static bool firstResampler = true;
    auto it = resamplers.find(type);
    [[maybe_unused]] auto channels =
        exo::channelCount(sourcePcmFormat.channels);
    [[maybe_unused]] auto sourceRate = sourcePcmFormat.rate;

    if (it != resamplers.end()) {
        switch (it->second) {
#define RESAMPLER_DEF(N, T)                                                    \
    case exo::ResamplerImpl::N:                                                \
        return std::make_unique<exo::MultiChannelResamplerImpl<T>>(            \
            channels, targetRate, sourceRate, config);
            RESAMPLER_DEFS
#undef RESAMPLER_DEF
        default:
            break;
        }
    }

    if (type.empty()) {
#define RESAMPLER_DEF(N, T)                                                    \
    static bool try_##N = true;                                                \
    try {                                                                      \
        if (try_##N) {                                                         \
            auto ptr = std::make_unique<exo::MultiChannelResamplerImpl<T>>(    \
                channels, targetRate, sourceRate, config);                     \
            if (firstResampler)                                                \
                EXO_LOG("using resampler '" EXO_STRINGIFY(N) "'");             \
            firstResampler = false;                                            \
            return ptr;                                                        \
        }                                                                      \
    } catch (const std::exception& e) {                                        \
        try_##N = false;                                                       \
        if (firstResampler)                                                    \
            EXO_LOG("could not create resampler '" EXO_STRINGIFY(N) "': %s",   \
                    e.what());                                                 \
    }
        RESAMPLER_DEFS
#undef RESAMPLER_DEF
    }

    throw exo::UnknownResamplerError("unknown encoder '" + type + "'");
}

void printResamplerOptions(std::ostream& stream) {
    std::vector<std::string> options;
    for (const auto& [key, _] : resamplers)
        options.push_back(key);

    std::sort(options.begin(), options.end());
    for (const auto& key : options)
        std::cout << " " << key;
}

} // namespace exo
