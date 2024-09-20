/***
exocaster -- audio streaming helper
encoder/libopus/oggopus.hh -- Ogg Opus encoder using libopus

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

#ifndef ENCODER_LIBOPUS_OGGOPUS_HH
#define ENCODER_LIBOPUS_OGGOPUS_HH

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

#include "config.hh"
#include "encoder/encoder.hh"
#include "encoder/ogg/ogg.hh"
#include "metadata.hh"
#include "resampler/resampler.hh"
#include "slot.hh"
#include "streamformat.hh"
#include "types.hh"
#include "util.hh"

extern "C" {
#include <ogg/ogg.h>
#include <opus/opus.h>
#include <opus/opus_types.h>
}

namespace exo {

struct OpusEncoder : public PointerSlot<OpusEncoder, ::OpusEncoder> {
    using PointerSlot::PointerSlot;
    OpusEncoder(opus_int32 Fs, int channels, int application);
    ~OpusEncoder() noexcept;
    EXO_DEFAULT_NONCOPYABLE(OpusEncoder)
};

static constexpr std::size_t MAX_OPUS_PACKET_SIZE = 1276 * 4;

class OggOpusEncoder : public exo::BaseEncoder {
    exo::OpusEncoder encoder_;
    std::unique_ptr<exo::OggStreamState> stream_;
    std::uint32_t serial_;
    bool init_{false};
    bool endOfStream_{false};
    std::size_t granulesInPage_{0};
    std::uint_least64_t lastGranulePosition_{0};
    std::size_t packetIndex_{0};
    int channels_;
    std::int_least32_t bitrate_;
    int complexity_;
    std::vector<float> pcm_;
    std::vector<exo::byte> opus_;
    unsigned long rate_;
    std::size_t pcmIndex_{0};
    std::size_t granuleIndex_{0};
    std::unique_ptr<exo::BaseMultiChannelResampler> resampler_;
    std::vector<float> mid_;
    exo::byte lastToc_;

    void pushPage_(const ogg_page& page);
    ogg_packet makeOggPacket_(std::span<byte> data, bool eos = false);
    void flushResampler_();
    void flushResampler_(std::span<const float> samples);
    void flushBuffer_(bool force);
    void flushPages_();
    std::size_t makeFinalOpusFrame_();

  public:
    OggOpusEncoder(const exo::ConfigObject& config,
                   std::shared_ptr<exo::PcmBuffer> source,
                   exo::PcmFormat pcmFormat,
                   const exo::ResamplerFactory& resamplerFactory,
                   const std::shared_ptr<exo::Barrier>& barrier);

    exo::StreamFormat streamFormat() const noexcept;
    std::size_t outputFrameRate() const noexcept;

    void startTrack(const exo::Metadata& metadata);
    void pcmBlock(std::size_t frameCount, std::span<const exo::byte> data);
    void endTrack();
};

} // namespace exo

#endif /* ENCODER_LIBOPUS_OGGOPUS_HH */
