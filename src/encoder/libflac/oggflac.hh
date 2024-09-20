/***
exocaster -- audio streaming helper
encoder/libflac/oggflac.hh -- Ogg FLAC encoder using libFLAC

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

#ifndef ENCODER_LIBFLAC_OGGFLAC_HH
#define ENCODER_LIBFLAC_OGGFLAC_HH

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
#include <FLAC/format.h>
#include <FLAC/stream_encoder.h>
}

/* libFLAC write callback samples is broken for Ogg FLAC.
    <https://github.com/xiph/flac/issues/661>
    <https://github.com/xiph/flac/pull/743> */
#ifndef EXO_OGGFLAC_SAMPLES_HACK
#define EXO_OGGFLAC_SAMPLES_HACK 0
#endif

namespace exo {

struct FlacStreamEncoder
    : public PointerSlot<FlacStreamEncoder, FLAC__StreamEncoder> {
    using PointerSlot::PointerSlot;
    FlacStreamEncoder();
    ~FlacStreamEncoder() noexcept;
    EXO_DEFAULT_NONCOPYABLE(FlacStreamEncoder)
};

struct FlacStreamMetadata
    : public PointerSlot<FlacStreamMetadata, FLAC__StreamMetadata> {
    using PointerSlot::PointerSlot;
    FlacStreamMetadata(FLAC__MetadataType type);
    ~FlacStreamMetadata() noexcept;
    EXO_DEFAULT_NONCOPYABLE(FlacStreamMetadata)
};

class OggFlacEncoder : public exo::BaseEncoder {
    FlacStreamEncoder encoder_;
    FlacStreamMetadata metadata_;
    std::uint32_t serial_;
    bool init_{false};
    unsigned channels_;
    exo::PcmSampleFormat flacSampleFormat_;
    std::vector<exo::byte> pcmBuffer_;
    unsigned level_;
#if EXO_OGGFLAC_SAMPLES_HACK
    std::uint64_t lastGranulePos_{0};
#endif

  public:
    OggFlacEncoder(const exo::ConfigObject& config,
                   std::shared_ptr<exo::PcmBuffer> source,
                   exo::PcmFormat pcmFormat,
                   const exo::ResamplerFactory& resamplerFactory,
                   const std::shared_ptr<exo::Barrier>& barrier);

    exo::StreamFormat streamFormat() const noexcept;

    void startTrack(const exo::Metadata& metadata);
    void pcmBlock(std::size_t frameCount, std::span<const exo::byte> data);
    void endTrack();

    FLAC__StreamEncoderWriteStatus writeCallback(const FLAC__byte buffer[],
                                                 std::size_t bytes,
                                                 std::uint32_t samples,
                                                 std::uint32_t currentFrame);
};

} // namespace exo

#endif /* ENCODER_LIBFLAC_OGGFLAC_HH */
