/***
exocaster -- audio streaming helper
decoder/libavcodec/lavc.hh -- libavcodec powered decoder

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

#ifndef DECODER_LIBAVCODEC_LAVC_HH
#define DECODER_LIBAVCODEC_LAVC_HH

#include "decoder/decoder.hh"
#include "slot.hh"

#define USE_LIBAVFILTER 0

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#if USE_LIBAVFILTER
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#else
#include <libswresample/swresample.h>
#endif
}

namespace exo {

struct LavcDecodeParams {
    bool applyReplayGain;
    double replayGainPreamp;
    bool replayGainAntipeak;
    bool r128Fix;
    bool normalizeVorbisComment;
};

#if !USE_LIBAVFILTER
union LavcGain {
    std::int_least32_t i;
    double f;
};

class LavcGainCalculator {
    unsigned short hasMask_{0};
    unsigned short rejectMask_{0};
    float replayGain_;
    float replayGainPeak_;
    float r128Gain_;

    bool has_(unsigned short mask) const noexcept;
    bool accepts_(unsigned short mask) noexcept;

  public:
    void accept() noexcept;
    void replayGain(float value) noexcept;
    void replayGainPeak(float value) noexcept;
    void r128Gain(float value) noexcept;

    std::optional<double> gain(bool antipeak, double preamp) const noexcept;
};
#endif

struct LavPacket : public PointerSlot<LavPacket, AVPacket> {
    using PointerSlot::PointerSlot;
    LavPacket();
    ~LavPacket() noexcept;
    EXO_DEFAULT_NONCOPYABLE(LavPacket)
};

struct LavFrame : public PointerSlot<LavFrame, AVFrame> {
    using PointerSlot::PointerSlot;
    LavFrame();
    ~LavFrame() noexcept;
    EXO_DEFAULT_NONCOPYABLE(LavFrame)
};

struct LavFormatInput : public PointerSlot<LavFormatInput, AVFormatContext> {
    using PointerSlot::PointerSlot;
    LavFormatInput(AVFormatContext* p);
    ~LavFormatInput() noexcept;
    EXO_DEFAULT_NONCOPYABLE(LavFormatInput)
};

struct LavCodecContext : public PointerSlot<LavCodecContext, AVCodecContext> {
    using PointerSlot::PointerSlot;
    LavCodecContext(const AVCodec* codec);
    ~LavCodecContext() noexcept;
    EXO_DEFAULT_NONCOPYABLE(LavCodecContext)
};

#if USE_LIBAVFILTER

struct LavFilterContext
    : public PointerSlot<LavFilterContext, AVFilterContext> {
    using PointerSlot::PointerSlot;
    LavFilterContext(AVFilterContext* p);
    ~LavFilterContext() noexcept;
    EXO_DEFAULT_NONCOPYABLE(LavFilterContext)
};

struct LavFilterGraph : public PointerSlot<LavFilterGraph, AVFilterGraph> {
    using PointerSlot::PointerSlot;
    LavFilterGraph();
    ~LavFilterGraph() noexcept;
    EXO_DEFAULT_NONCOPYABLE(LavFilterGraph)
};

#else /* USE_LIBAVFILTER */

struct LavSwrContext : public PointerSlot<LavSwrContext, SwrContext> {
    using PointerSlot::PointerSlot;
    LavSwrContext(SwrContext* p);
    ~LavSwrContext() noexcept;
    EXO_DEFAULT_NONCOPYABLE(LavSwrContext)
};

#endif /* USE_LIBAVFILTER */

class LavcDecodeJob : public exo::BaseDecodeJob {
    std::string filePath_;
    exo::LavcDecodeParams params_;
    bool canPlay_{false};
    exo::LavPacket packet_;
    exo::LavFrame frame_;
    exo::LavFormatInput formatContext_{nullptr};
    exo::LavCodecContext codecContext_{nullptr};
#if USE_LIBAVFILTER
    AVFilterContext* bufferSourceContext_{nullptr};
    AVFilterContext* filterSinkContext_{nullptr};
    exo::LavFilterGraph filterGraph_{nullptr};
    exo::LavFrame filterFrame_;
    bool hasReplayGain_{false}, hasR128Gain_{false};
#else
    exo::LavcGain gain_;
    exo::LavcGainCalculator gainCalculator_;
    exo::LavSwrContext resamplerContext_{nullptr};
    int bufferFrameCount_;
    exo::byte* buffer_;
#endif
    AVChannelLayout outChLayout_;
    enum AVSampleFormat outSampleFmt_;
    exo::Metadata metadata_;
    int streamIndex_;

    void readMetadata_(const AVDictionary* metadict);
    int decodeFrames_(std::shared_ptr<exo::PcmSplitter>& sink,
                      bool flush = false);
#if USE_LIBAVFILTER
    bool setupFilter_();
    int filterFrames_(std::shared_ptr<exo::PcmSplitter>& sink,
                      bool flush = false);
#else
    void calculateGain_();
    int processFrame_(std::shared_ptr<exo::PcmSplitter>& sink,
                      const AVFrame* frame);
    int processBuffer_(std::shared_ptr<exo::PcmSplitter>& sink,
                       exo::byte* buffer, std::size_t frameCount);
#endif
    void flush_(std::shared_ptr<exo::PcmSplitter>& sink);

  public:
    LavcDecodeJob(std::shared_ptr<exo::PcmSplitter> sink,
                  exo::PcmFormat pcmFormat,
                  std::shared_ptr<exo::ConfigObject> command,
                  const std::string& filePath,
                  const exo::LavcDecodeParams& params);
    EXO_DEFAULT_NONCOPYABLE(LavcDecodeJob)
    ~LavcDecodeJob() noexcept;

    void init();
    void run(std::shared_ptr<exo::PcmSplitter> sink);
};

class LavcDecoder : public exo::BaseDecoder {
    exo::LavcDecodeParams params_;

  public:
    LavcDecoder(const exo::ConfigObject& config, exo::PcmFormat pcmFormat);

    std::optional<std::unique_ptr<BaseDecodeJob>>
    createJob(const exo::ConfigObject& request,
              std::shared_ptr<exo::ConfigObject> command);
};

} // namespace exo

#endif /* DECODER_LIBAVCODEC_LAVC_HH */
