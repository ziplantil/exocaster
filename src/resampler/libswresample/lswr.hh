/***
exocaster -- audio streaming helper
resampler/libswresample/lswr.hh -- libswresample resampler

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

#ifndef RESAMPLER_LIBSWRESAMPLE_LSWR_HH
#define RESAMPLER_LIBSWRESAMPLE_LSWR_HH

#include "config.hh"
#include "resampler/types.hh"
#include "slot.hh"
#include "util.hh"

extern "C" {
#include <libswresample/swresample.h>
}

namespace exo {

struct SwrContext : public PointerSlot<SwrContext, ::SwrContext> {
    using PointerSlot::PointerSlot;
    SwrContext(unsigned inRate, unsigned outRate, unsigned numChannels);
    ~SwrContext() noexcept;
    EXO_DEFAULT_NONCOPYABLE(SwrContext)
};

class LswrResampler {
    SwrContext swr_;
    double ratio_;
    bool noskip_{false};
    bool reset_{false};

  public:
    LswrResampler(exo::SampleRate outRate, exo::SampleRate inRate,
                  const exo::ConfigObject& config);
    EXO_DEFAULT_NONCOPYABLE_DEFAULT_DESTRUCTOR(LswrResampler);

    exo::ResamplerReturn resample(std::span<float> out,
                                  std::span<const float> in);
    std::size_t flush(std::span<float> out);
};

static_assert(exo::ResamplerConcept<exo::LswrResampler>);

} // namespace exo

#endif /* RESAMPLER_LIBSWRESAMPLE_LSWR_HH */
