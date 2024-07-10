/***
exocaster -- audio streaming helper
encoder/pcm.hh -- PCM passthrough encoder

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

#ifndef ENCODER_PCM_HH
#define ENCODER_PCM_HH

#include <cstddef>
#include <memory>
#include <span>

#include "config.hh"
#include "encoder/encoder.hh"
#include "metadata.hh"
#include "pcmtypes.hh"
#include "resampler/resampler.hh"
#include "streamformat.hh"
#include "types.hh"

namespace exo {

class PcmEncoder : public exo::BaseEncoder {
    bool metadata_;

  public:
    PcmEncoder(const exo::ConfigObject& config,
               std::shared_ptr<exo::PcmBuffer> source, exo::PcmFormat pcmFormat,
               const exo::ResamplerFactory& resamplerFactory);

    exo::StreamFormat streamFormat() const noexcept;

    void startTrack(const exo::Metadata& metadata);
    void pcmBlock(std::size_t frameCount, std::span<const exo::byte> data);
};

} // namespace exo

#endif /* ENCODER_PCM_HH */
