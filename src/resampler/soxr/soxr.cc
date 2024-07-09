/***
exocaster -- audio streaming helper
resampler/soxr/soxr.cc -- SoX resampler

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

#include <stdexcept>

#include "debug.hh"
#include "log.hh"
#include "resampler/soxr/soxr.hh"

extern "C" {
#include <soxr.h>
}

namespace exo {

Soxr::Soxr(double inRate, double outRate, unsigned numChannels)
    : PointerSlot(nullptr) {
    soxr_error_t error;
    soxr_io_spec_t io_spec = soxr_io_spec(SOXR_FLOAT32_I, SOXR_FLOAT32_I);
    set() = soxr_create(inRate, outRate, numChannels, &error, &io_spec, nullptr,
                        nullptr);
    if (error) {
        EXO_LOG("soxr_create failed: %s", error);
        throw std::runtime_error("soxr_create failed");
    }
}

Soxr::~Soxr() noexcept {
    if (has())
        soxr_delete(release());
}

SoxResampler::SoxResampler(exo::SampleRate outRate, exo::SampleRate inRate,
                           const exo::ConfigObject& config)
    : soxr_(inRate, outRate, 1) {}

exo::ResamplerReturn SoxResampler::resample(std::span<float> out,
                                            std::span<const float> in) {
    soxr_error_t error;
    std::size_t inDone, outDone;
    const float* ptr = in.data();
    float zero = 0;
    if (!ptr) {
        EXO_ASSERT(in.size() == 0);
        // don't flush!
        ptr = &zero;
    }
    if (reset_) {
        soxr_clear(soxr_.get());
        reset_ = false;
    }
    error = soxr_process(soxr_.get(), ptr, in.size(), &inDone, out.data(),
                         out.size(), &outDone);
    if (error) {
        EXO_LOG("soxr_process failed: %s", error);
        return {.wrote = 0, .read = in.size()};
    }
    return {.wrote = outDone, .read = inDone};
}

std::size_t SoxResampler::flush(std::span<float> out) {
    soxr_error_t error;
    std::size_t outDone;
    reset_ = true;
    error = soxr_process(soxr_.get(), nullptr, 0, nullptr, out.data(),
                         out.size(), &outDone);
    if (error) {
        EXO_LOG("soxr_process failed: %s", error);
        return 0;
    }
    return outDone;
}

} // namespace exo
