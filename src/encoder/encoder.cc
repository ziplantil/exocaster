/***
exocaster -- audio streaming helper
encoder/encoder.cc -- encoder framework

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
#include <array>
#include <unordered_map>
#include <utility>
#include <vector>

#include "encoder/pcm.hh"
#include "log.hh"
#include "packet.hh"
#include "pcmbuffer.hh"
#include "server.hh"

#include "encoder/encoder.hh"

#if EXO_LIBVORBIS
#include "encoder/libvorbis/oggvorbis.hh"
#define ENCODER_DEFS_LIBVORBIS ENCODER_DEF(oggvorbis, exo::OggVorbisEncoder)
#else
#define ENCODER_DEFS_LIBVORBIS
#endif

#if EXO_LIBFLAC
#include "encoder/libflac/oggflac.hh"
#define ENCODER_DEFS_LIBFLAC ENCODER_DEF(oggflac, exo::OggFlacEncoder)
#else
#define ENCODER_DEFS_LIBFLAC
#endif

#if EXO_LIBOPUS
#include "encoder/libopus/oggopus.hh"
#define ENCODER_DEFS_LIBOPUS ENCODER_DEF(oggopus, exo::OggOpusEncoder)
#else
#define ENCODER_DEFS_LIBOPUS
#endif

#if EXO_MP3LAME
#include "encoder/lame/mp3.hh"
#define ENCODER_DEFS_MP3LAME ENCODER_DEF(mp3, exo::Mp3Encoder)
#else
#define ENCODER_DEFS_MP3LAME
#endif

namespace exo {

#define ENCODER_DEFS                                                           \
    ENCODER_DEF(pcm, exo::PcmEncoder)                                          \
    ENCODER_DEFS_LIBVORBIS                                                     \
    ENCODER_DEFS_LIBFLAC                                                       \
    ENCODER_DEFS_LIBOPUS                                                       \
    ENCODER_DEFS_MP3LAME

enum class EncoderImpl {
#define ENCODER_DEF(name, T) name,
    ENCODER_DEFS
#undef ENCODER_DEF
};

static std::unordered_map<std::string, exo::EncoderImpl> encoders = {
#define ENCODER_DEF(N, T) {#N, exo::EncoderImpl::N},
    ENCODER_DEFS
#undef ENCODER_DEF
};

std::unique_ptr<exo::BaseEncoder>
createEncoder(const std::string& type, const exo::ConfigObject& config,
              std::shared_ptr<exo::PcmBuffer> source, exo::PcmFormat pcmFormat,
              const exo::ResamplerFactory& resamplerFactory) {
    auto it = encoders.find(type);

    if (it != encoders.end()) {
        switch (it->second) {
#define ENCODER_DEF(N, T)                                                      \
    case exo::EncoderImpl::N:                                                  \
        return std::make_unique<T>(config, source, pcmFormat, resamplerFactory);
            ENCODER_DEFS
#undef ENCODER_DEF
        }
    }

    throw exo::UnknownEncoderError("unknown encoder '" + type + "'");
}

void BaseEncoder::packet(unsigned flags, std::size_t frameCount,
                         std::span<const exo::byte> data) {
    if (startOfTrack_)
        flags |= PacketFlags::StartOfTrack;
    for (auto& sink : sinks_)
        sink->writePacket(flags, frameCount, data);
    startOfTrack_ = false;
}

void BaseEncoder::packet(std::size_t frameCount,
                         std::span<const exo::byte> data) {
    packet(0, frameCount, data);
}

void BaseEncoder::run() {
    if (!source_)
        return;

    std::array<exo::byte, ENCODER_BUFFER> buffer;
    static_assert(buffer.size() >= exo::MAX_BYTES_PER_FRAME);
    bool track = false;

    while (EXO_LIKELY(exo::shouldRun())) {
        std::shared_ptr<exo::Metadata> metadataPtr = source_->readMetadata();
        if (metadataPtr) {
            if (track)
                endTrack();
            startTrack(*metadataPtr);
            startOfTrack_ = true;
            track = true;
            metadataPtr = nullptr;
        }

        auto t0 = std::chrono::steady_clock::now();
        std::size_t n = source_->readPcm(buffer);
        auto t1 = std::chrono::steady_clock::now();
        if (EXO_LIKELY(n)) {
            auto waitMs =
                std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0)
                    .count();
            if (waitMs >= 500)
                EXO_LOG("buffer underrun? waited %u ms", waitMs);
            pcmBlock(n / pcmFormat_.bytesPerFrame(),
                     std::span<exo::byte>(buffer.begin(), n));
        } else if (EXO_UNLIKELY(source_->closed())) {
            break;
        }
    }

    if (track)
        endTrack();
    close();
}

void BaseEncoder::close() {
    for (auto& sink : sinks_)
        sink->close();
}

void printEncoderOptions(std::ostream& stream) {
    std::vector<std::string> options;
    for (const auto& [key, _] : encoders)
        options.push_back(key);

    std::sort(options.begin(), options.end());
    for (const auto& key : options)
        std::cout << " " << key;
}

} // namespace exo
