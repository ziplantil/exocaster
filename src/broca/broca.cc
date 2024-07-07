/***
exocaster -- audio streaming helper
broca/broca.cc -- broca (broadcaster) framework

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

#include <algorithm>
#include <unordered_map>
#include <vector>

#include "broca/broca.hh"
#include "broca/discard.hh"
#include "broca/file.hh"

#if EXO_PORTAUDIO
#include "broca/portaudio/playback.hh"
#define BROCA_DEFS_PORTAUDIO                                                   \
    BROCA_DEF(portaudio, exo::PortAudioBroca)
#else
#define BROCA_DEFS_PORTAUDIO
#endif

#if EXO_SHOUT
#include "broca/shout/shout.hh"
#define BROCA_DEFS_SHOUT                                                       \
    BROCA_DEF(shout, exo::ShoutBroca)
#else
#define BROCA_DEFS_SHOUT
#endif

namespace exo {

#define BROCA_DEFS                                                             \
    BROCA_DEF(discard, exo::DiscardBroca)                                      \
    BROCA_DEF(file, exo::FileBroca)                                            \
    BROCA_DEFS_PORTAUDIO                                                       \
    BROCA_DEFS_SHOUT

enum class BrocaImpl {
#define BROCA_DEF(name, T) name,
    BROCA_DEFS
#undef BROCA_DEF
};

static std::unordered_map<std::string, exo::BrocaImpl> brocas = {
#define BROCA_DEF(N, T) { #N, exo::BrocaImpl::N },
    BROCA_DEFS
#undef BROCA_DEF
};

std::unique_ptr<exo::BaseBroca> createBroca(
            const std::string& type,
            const exo::ConfigObject& config,
            std::shared_ptr<exo::PacketRingBuffer> source,
            const exo::StreamFormat& streamFormat,
            const exo::PcmFormat& pcmFormat) {
    auto it = brocas.find(type);

    if (it != brocas.end()) {
        switch (it->second) {
#define BROCA_DEF(N, T)                                                        \
        case exo::BrocaImpl::N:                                                \
            return std::make_unique<T>(config, source, streamFormat,           \
                                       pcmFormat.rate);
        BROCA_DEFS
#undef BROCA_DEF
        }
    }

    throw exo::UnknownBrocaError("unknown broca '" + type + "'");
}

struct BrocaWatchdog {
    ~BrocaWatchdog() {
        exo::brocaCounter.release();
    }
};

void BaseBroca::run() {
    BrocaWatchdog watchdog;
    runImpl();
}

void printBrocaOptions(std::ostream& stream) {
    std::vector<std::string> options;
    for (const auto& [key, _]: brocas)
        options.push_back(key);

    std::sort(options.begin(), options.end());
    for (const auto& key: options)
        std::cout << " " << key;
}

} // namespace exo
