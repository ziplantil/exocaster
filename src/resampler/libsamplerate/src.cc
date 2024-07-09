/***
exocaster -- audio streaming helper
resampler/libsamplerate/src.cc -- Secret Rabbit Code resampler

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

#include "log.hh"
#include "resampler/libsamplerate/src.hh"

extern "C" {
#include <samplerate.h>
}

namespace exo {

SrcState::SrcState(unsigned numChannels) : PointerSlot(nullptr) {
    int error;
    set() = src_new(SRC_SINC_BEST_QUALITY, numChannels, &error);
    if (error) {
        EXO_LOG("src_new failed: %s", src_strerror(error));
        throw std::runtime_error("src_new failed");
    }
}

SrcState::~SrcState() noexcept {
    if (has())
        src_delete(release());
}

SrcResampler::SrcResampler(exo::SampleRate outRate, exo::SampleRate inRate,
                           const exo::ConfigObject& config)
    : src_(1), ratio_(static_cast<double>(outRate) / inRate) {}

exo::ResamplerReturn SrcResampler::resample(std::span<float> out,
                                            std::span<const float> in) {
    SRC_DATA data;
    if (reset_) {
        src_reset(src_.get());
        reset_ = false;
    }
    data.data_in = in.data();
    data.data_out = out.data();
    data.input_frames = in.size();
    data.output_frames = out.size();
    data.end_of_input = 0;
    data.src_ratio = ratio_;
    int err = src_process(src_.get(), &data);
    if (err) {
        EXO_LOG("src_process failed: %s", src_strerror(err));
        return {.wrote = 0, .read = in.size()};
    }
    return {.wrote = static_cast<std::size_t>(data.output_frames_gen),
            .read = static_cast<std::size_t>(data.input_frames_used)};
}

std::size_t SrcResampler::flush(std::span<float> out) {
    SRC_DATA data;
    data.data_in = nullptr;
    data.data_out = out.data();
    data.input_frames = 0;
    data.output_frames = out.size();
    data.end_of_input = 1;
    data.src_ratio = ratio_;
    reset_ = true;
    int err = src_process(src_.get(), &data);
    if (err) {
        EXO_LOG("src_process failed: %s", src_strerror(err));
        return 0;
    }
    return static_cast<std::size_t>(data.output_frames_gen);
}

} // namespace exo
