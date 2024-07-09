/***
exocaster -- audio streaming helper
resampler/resampler.hh -- multi-channel resampler

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

#ifndef RESAMPLER_RESAMPLER_HH
#define RESAMPLER_RESAMPLER_HH

#include <algorithm>
#include <memory>
#include <span>
#include <vector>

#include "config.hh"
#include "pcmtypes.hh"
#include "resampler/types.hh"
#include "util.hh"

namespace exo {

class BaseMultiChannelResampler {
  public:
    virtual ~BaseMultiChannelResampler() = default;

    /** Resamples floating-point PCM samples given in src to dst.

        The input and outputs are planar. This means that the number of frames
        in the buffer is divided by the number of channels. For example,
        with stereo audio, if the buffer contains 240 samples, then the first
        120 samples are for the left channel, while the second half is for
        the right channel.

        Returns the number of frames (sampler per channel) read
        from the input and written to the output.

        Both buffers are expected to have a capacity that is a multiple of
        the number of channels. For example, with 2 channels, both buffers
        must have an even number of samples. All channels are also assumed
        to have the same number of samples in the buffer.

        The resampler may not read all samples from the source buffer. This
        may happen if the internal buffer in the resampler is full and cannot
        admit any more samples. Users must check the read field of the
        return value to check how many samples from the source buffer
        were actually read.

        Likewise, if the destination buffer is too small, the resampler may
        not return all samples that it is able to. The resampler will also
        not write any partial samples, i.e. those that still need more data
        to resample properly. These can only be obtained by flushing. */
    virtual exo::ResamplerReturn resamplePlanar(std::span<float> dst,
                                                std::span<const float> src) = 0;

    /** Resamples floating-point PCM samples given in src to dst.

        The inputs and outputs are interleaved. This means for example that
        with stereo, the sample for the left channel is followed immediately
        by the sample in the right channel, before the next sample in the
        left channel.

        Returns the number of frames (sampler per channel) read
        from the input and written to the output.

        Both buffers are expected to have a capacity that is a multiple of
        the number of channels. For example, with 2 channels, both buffers
        must have an even number of samples.

        The resampler may not read all samples from the source buffer. This
        may happen if the internal buffer in the resampler is full and cannot
        admit any more samples. Users must check the read field of the
        return value to check how many samples from the source buffer
        were actually read.

        Likewise, if the destination buffer is too small, the resampler may
        not return all samples that it is able to. The resampler will also
        not write any partial samples, i.e. those that still need more data
        to resample properly. These can only be obtained by flushing. */
    virtual exo::ResamplerReturn
    resampleInterleaved(std::span<float> dst, std::span<const float> src) = 0;

    /** Yields any remaining partial samples from the resampler, with
        the assumption that any additional samples needed to establish
        their final value are silence.

        Returns the number of frames (sampler per channel)
        written to the buffer.

        The output is planar. This means that the number of frames in the
        buffer is divided by the number of channels. For example, with stereo
        audio, if the buffer contains 240 samples, then the first 120 samples
        are for the left channel, while the second half is for the right
        channel.

        The output buffer is expected to have a capacity that is a multiple of
        the number of channels. For example, with 2 channels, the buffer
        must have space for an even number of samples. Any additional space
        is not filled in. */
    virtual std::size_t flushPlanar(std::span<float> dst) = 0;

    /** Yields any remaining partial samples from the resampler, with
        the assumption that any additional samples needed to establish
        their final value are silence.

        Returns the number of frames (sampler per channel)
        written to the buffer.

        The output is interleaved. This means for example that with stereo,
        the sample for the left channel is followed immediately by the sample
        in the right channel, before the next sample in the left channel.

        The output buffer is expected to have a capacity that is a multiple of
        the number of channels. For example, with 2 channels, the buffer
        must have space for an even number of samples. Any additional space
        is not filled in. */
    virtual std::size_t flushInterleaved(std::span<float> dst) = 0;
};

template <exo::ResamplerConcept T>
class MultiChannelResamplerImpl : public BaseMultiChannelResampler {
    std::vector<T> resamplers_;
    std::vector<float> in_;
    std::vector<float> out_;
    std::size_t channels_;
    bool passThrough_;

  public:
    /** Constructs a multi-channel resampler with the given number of channels,
        output sample rate, input sample rate, and other parameters
        to the underlying resampler. */
    template <typename... Args>
    MultiChannelResamplerImpl(std::size_t channels, exo::SampleRate outRate,
                              exo::SampleRate inRate, Args&&... args)
        : channels_(channels),
          passThrough_(outRate == inRate || channels == 0) {
        if (passThrough_)
            return;
        resamplers_.reserve(channels);
        for (std::size_t i = 0; i < channels; ++i)
            resamplers_.push_back(
                T(outRate, inRate, std::forward<Args>(args)...));
    }

    /** Resamples floating-point PCM samples given in src to dst.

        The input and outputs are planar. This means that the number of frames
        in the buffer is divided by the number of channels. For example,
        with stereo audio, if the buffer contains 240 samples, then the first
        120 samples are for the left channel, while the second half is for
        the right channel.

        Returns the number of frames (sampler per channel) read
        from the input and written to the output.

        Both buffers are expected to have a capacity that is a multiple of
        the number of channels. For example, with 2 channels, both buffers
        must have an even number of samples. All channels are also assumed
        to have the same number of samples in the buffer.

        The resampler may not read all samples from the source buffer. This
        may happen if the internal buffer in the resampler is full and cannot
        admit any more samples. Users must check the read field of the
        return value to check how many samples from the source buffer
        were actually read.

        Likewise, if the destination buffer is too small, the resampler may
        not return all samples that it is able to. The resampler will also
        not write any partial samples, i.e. those that still need more data
        to resample properly. These can only be obtained by flushing. */
    exo::ResamplerReturn resamplePlanar(std::span<float> dst,
                                        std::span<const float> src) {
        if (passThrough_) {
            std::size_t size = std::min(dst.size(), src.size());
            std::copy(src.data(), src.data() + size, dst.data());
            auto frames = size / channels_;
            return {.wrote = frames, .read = frames};
        }

        auto stride = channels_;
        auto inFrameCount = src.size() / stride;
        auto outFrameSpace = dst.size() / stride;

        std::size_t inFrames = 0;
        std::size_t outFrames = outFrameSpace;
        float* d = dst.data();
        const float* s = src.data();

        for (auto& resampler : resamplers_) {
            exo::ResamplerReturn subresult =
                resampler.resample({d, outFrameSpace}, {s, inFrameCount});
            if (inFrames < subresult.read)
                inFrames = subresult.read;
            if (outFrames > subresult.wrote)
                outFrames = subresult.wrote;

            d += outFrameSpace;
            s += inFrameCount;
        }

        return {.wrote = outFrames, .read = inFrames};
    }

    /** Resamples floating-point PCM samples given in src to dst.

        The inputs and outputs are interleaved. This means for example that
        with stereo, the sample for the left channel is followed immediately
        by the sample in the right channel, before the next sample in the
        left channel.

        Returns the number of frames (sampler per channel) read
        from the input and written to the output.

        Both buffers are expected to have a capacity that is a multiple of
        the number of channels. For example, with 2 channels, both buffers
        must have an even number of samples.

        The resampler may not read all samples from the source buffer. This
        may happen if the internal buffer in the resampler is full and cannot
        admit any more samples. Users must check the read field of the
        return value to check how many samples from the source buffer
        were actually read.

        Likewise, if the destination buffer is too small, the resampler may
        not return all samples that it is able to. The resampler will also
        not write any partial samples, i.e. those that still need more data
        to resample properly. These can only be obtained by flushing. */
    exo::ResamplerReturn resampleInterleaved(std::span<float> dst,
                                             std::span<const float> src) {
        if (passThrough_) {
            std::size_t size = std::min(dst.size(), src.size());
            std::copy(src.data(), src.data() + size, dst.data());
            auto frames = size / channels_;
            return {.wrote = frames, .read = frames};
        }

        if (channels_ == 1)
            return resamplePlanar(dst, src);

        auto stride = channels_;
        auto inFrameCount = src.size() / stride;
        auto outFrameSpace = dst.size() / stride;

        in_.reserve(inFrameCount);
        out_.reserve(outFrameSpace);

        std::size_t inFrames = 0;
        std::size_t outFrames = outFrameSpace;
        std::size_t channel = 0;

        std::span<float> xin{in_.data(), inFrameCount};
        std::span<float> xout{out_.data(), outFrameSpace};

        for (auto& resampler : resamplers_) {
            // uninterleave
            const float* s = &src[channel];
            for (std::size_t i = 0; i < inFrameCount; ++i) {
                in_[i] = *s;
                s += stride;
            }

            // resample
            exo::ResamplerReturn subresult = resampler.resample(xout, xin);
            if (inFrames < subresult.read)
                inFrames = subresult.read;
            if (outFrames > subresult.wrote)
                outFrames = subresult.wrote;

            // interleave
            float* d = &dst[channel];
            for (std::size_t i = 0; i < outFrames; ++i) {
                *d = out_[i];
                d += stride;
            }
            ++channel;
        }

        return {.wrote = outFrames, .read = inFrames};
    }

    /** Yields any remaining partial samples from the resampler, with
        the assumption that any additional samples needed to establish
        their final value are silence.

        Returns the number of frames (sampler per channel)
        written to the buffer.

        The output is planar. This means that the number of frames in the
        buffer is divided by the number of channels. For example, with stereo
        audio, if the buffer contains 240 samples, then the first 120 samples
        are for the left channel, while the second half is for the right
        channel.

        The output buffer is expected to have a capacity that is a multiple of
        the number of channels. For example, with 2 channels, the buffer
        must have space for an even number of samples. Any additional space
        is not filled in. */
    std::size_t flushPlanar(std::span<float> dst) {
        if (passThrough_)
            // nothing to flush if we pass everything through
            return 0;

        auto stride = channels_;
        auto outFrameSpace = dst.size() / stride;

        std::size_t outFrames = outFrameSpace;
        float* d = dst.data();
        for (auto& resampler : resamplers_) {
            std::size_t samples = resampler.flush({d, outFrameSpace});
            if (outFrames > samples)
                outFrames = samples;
            d += outFrameSpace;
        }

        return outFrames;
    }

    /** Yields any remaining partial samples from the resampler, with
        the assumption that any additional samples needed to establish
        their final value are silence.

        Returns the number of frames (sampler per channel)
        written to the buffer.

        The output is interleaved. This means for example that with stereo,
        the sample for the left channel is followed immediately by the sample
        in the right channel, before the next sample in the left channel.

        The output buffer is expected to have a capacity that is a multiple of
        the number of channels. For example, with 2 channels, the buffer
        must have space for an even number of samples. Any additional space
        is not filled in. */
    std::size_t flushInterleaved(std::span<float> dst) {
        if (passThrough_)
            // nothing to flush if we pass everything through
            return 0;

        if (channels_ == 1)
            return flushPlanar(dst);

        auto stride = channels_;
        auto outFrameSpace = dst.size() / stride;
        out_.reserve(outFrameSpace);

        std::size_t outFrames = outFrameSpace;
        std::size_t channel = 0;
        std::span<float> xout{out_.data(), outFrameSpace};
        for (auto& resampler : resamplers_) {
            // flush
            std::size_t samples = resampler.flush(xout);
            if (outFrames > samples)
                outFrames = samples;

            // interleave
            float* d = &dst[channel++];
            for (std::size_t i = 0; i < outFrames; ++i) {
                *d = out_[i];
                d += stride;
            }
        }

        return outFrames;
    }
};

class UnknownResamplerError : public std::logic_error {
  public:
    using std::logic_error::logic_error;
};

std::unique_ptr<exo::BaseMultiChannelResampler>
createResampler(const std::string& type, const exo::ConfigObject& config,
                exo::PcmFormat sourcePcmFormat, exo::SampleRate targetRate);

struct ResamplerFactory {
    virtual std::unique_ptr<exo::BaseMultiChannelResampler>
    createResampler(exo::SampleRate targetRate) const = 0;
};

class StandardResamplerFactory : public ResamplerFactory {
    std::string type_;
    const exo::ConfigObject& config_;
    exo::PcmFormat sourcePcmFormat_;

  public:
    inline StandardResamplerFactory(std::string type,
                                    const exo::ConfigObject& config,
                                    exo::PcmFormat sourcePcmFormat)
        : type_(type), config_(config), sourcePcmFormat_(sourcePcmFormat) {}
    EXO_DEFAULT_NONMOVABLE(StandardResamplerFactory)

    inline std::unique_ptr<exo::BaseMultiChannelResampler>
    createResampler(exo::SampleRate targetRate) const {
        return exo::createResampler(type_, config_, sourcePcmFormat_,
                                    targetRate);
    }
};

void printResamplerOptions(std::ostream& stream);

} // namespace exo

#endif /* RESAMPLER_RESAMPLER_HH */
