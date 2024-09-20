/***
exocaster -- audio streaming helper
encoder/lame/mp3.hh -- MP3 encoder using LAME

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

#ifndef ENCODER_LAME_MP3_HH
#define ENCODER_LAME_MP3_HH

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

#include "config.hh"
#include "encoder/encoder.hh"
#include "metadata.hh"
#include "pcmbuffer.hh"
#include "resampler/resampler.hh"
#include "slot.hh"
#include "streamformat.hh"
#include "types.hh"
#include "util.hh"

extern "C" {
#include <lame/lame.h>
}

namespace exo {

struct Lame : public PointerSlot<Lame, lame_global_flags> {
    using PointerSlot::PointerSlot;
    Lame();
    ~Lame() noexcept;
    EXO_DEFAULT_NONCOPYABLE(Lame)
};

class Mp3Encoder : public exo::BaseEncoder {
    Lame lame_;
    bool init_{false}, vbr_;
    int channels_;
    std::int_least32_t minBitrate_, nomBitrate_, maxBitrate_;
    std::vector<exo::byte> buffer_;
    std::size_t granule_;

  public:
    Mp3Encoder(const exo::ConfigObject& config,
               std::shared_ptr<exo::PcmBuffer> source, exo::PcmFormat pcmFormat,
               const exo::ResamplerFactory& resamplerFactory,
               const std::shared_ptr<exo::Barrier>& barrier);

    exo::StreamFormat streamFormat() const noexcept;

    void startTrack(const exo::Metadata& metadata);
    void pcmBlock(std::size_t frameCount, std::span<const exo::byte> data);
    void endTrack();
};

} // namespace exo

#endif /* ENCODER_LAME_MP3_HH */
