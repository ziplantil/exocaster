/***
exocaster -- audio streaming helper
resampler/libswresample/lswr.cc -- libswresample resampler

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

#include <cstdint>
#include <stdexcept>

#include "debug.hh"
#include "log.hh"
#include "resampler/libswresample/lswr.hh"

extern "C" {
#include <libavutil/channel_layout.h>
#include <libavutil/error.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
}

namespace exo {

static void swrError_(const char* file, std::size_t lineno, const char* fn,
                      int ret) {
    char errbuf[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(ret, errbuf, sizeof(errbuf));
    exo::log(file, lineno, "%s failed (%d): %s", fn, ret, errbuf);
}

#define EXO_SWR_ERROR(fn, ret) exo::swrError_(__FILE__, __LINE__, fn, ret)

SwrContext::SwrContext(unsigned inRate, unsigned outRate, unsigned numChannels)
    : PointerSlot(nullptr) {
    int err;

    AVChannelLayout mono = AV_CHANNEL_LAYOUT_MONO;
    err = swr_alloc_set_opts2(&modify(), &mono, AV_SAMPLE_FMT_FLT, outRate,
                              &mono, AV_SAMPLE_FMT_FLT, inRate, 0, nullptr);
    if (err < 0) {
        EXO_SWR_ERROR("swr_alloc_set_opts2", err);
        throw std::runtime_error("swr_alloc_set_opts2 failed");
    }

    auto p = get();
    err = swr_init(p);
    if (err < 0) {
        EXO_SWR_ERROR("swr_init", err);
        throw std::runtime_error("swr_init failed");
    }
}

SwrContext::~SwrContext() noexcept {
    if (has()) {
        auto p = release();
        swr_close(p);
        swr_free(&p);
    }
}

LswrResampler::LswrResampler(exo::SampleRate outRate, exo::SampleRate inRate,
                             const exo::ConfigObject& config)
    : swr_(inRate, outRate, 1) {}

exo::ResamplerReturn LswrResampler::resample(std::span<float> out,
                                             std::span<const float> in) {
    auto ctx = swr_.get();
    int ret;
    const float* ptr = in.data();
    float zero = 0;
    if (!ptr) {
        EXO_ASSERT(in.size() == 0);
        // don't flush!
        ptr = &zero;
    }

    if (reset_) {
        swr_close(ctx);
        ret = swr_init(ctx);
        if (ret < 0) {
            EXO_SWR_ERROR("swr_init", ret);
            return {.wrote = 0, .read = in.size()};
        }
        reset_ = false;
    }

    auto src = reinterpret_cast<const std::uint8_t*>(ptr);
    auto dst = reinterpret_cast<std::uint8_t*>(out.data());
    auto zro = reinterpret_cast<const std::uint8_t*>(&zero);
    std::size_t readSamples;

    ret = swr_get_out_samples(ctx, 0);
    bool skip =
        ret >= 0 && static_cast<std::size_t>(ret) >= out.size() && !noskip_;
    auto dstn = out.size();

    std::size_t wrote = 0;
    if (!skip) {
        if ((ret = swr_convert(ctx, &dst, dstn, &src, in.size())) < 0) {
            return {.wrote = 0, .read = in.size()};
        }
        readSamples = in.size();
        wrote += ret, dst += ret, dstn -= ret;
    } else {
        readSamples = 0;
    }

    if (dstn > 0 && (ret = swr_convert(ctx, &dst, dstn, &zro, 0)) < 0) {
        return {.wrote = 0, .read = in.size()};
    }
    // swr_get_out_samples lies sometimes.
    // don't skip twice in a row if it told us we would get enough samples
    // but then we didn't
    noskip_ = skip && !ret && out.size();
    wrote += ret;

    return {.wrote = wrote, .read = readSamples};
}

std::size_t LswrResampler::flush(std::span<float> out) {
    auto ctx = swr_.get();
    int ret;
    auto dst = reinterpret_cast<std::uint8_t*>(out.data());

    reset_ = true;
    if ((ret = swr_convert(ctx, &dst, out.size(), nullptr, 0)) < 0) {
        EXO_SWR_ERROR("swr_convert(flush)", ret);
        return 0;
    }

    return static_cast<std::size_t>(ret);
}

} // namespace exo
