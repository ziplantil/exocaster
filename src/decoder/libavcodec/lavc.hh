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

#ifndef EXO_USE_LIBAVFILTER
/** Whether to use libavfilter for ReplayGain application and resampling */
#define EXO_USE_LIBAVFILTER 1
#endif

#include <memory>
#include <optional>
#include <string>
#if !EXO_USE_LIBAVFILTER
#include <cstddef>
#include <cstdint>
#endif

#include "config.hh"
#include "decoder/decoder.hh"
#include "metadata.hh"
#include "slot.hh"
#include "util.hh"
#if !EXO_USE_LIBAVFILTER
#include "types.hh"
#endif

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavcodec/codec.h>
#include <libavcodec/packet.h>
#include <libavformat/avformat.h>
#if EXO_USE_LIBAVFILTER
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavformat/avio.h>
#else
#include <libswresample/swresample.h>
#endif
#include <libavutil/channel_layout.h>
#include <libavutil/dict.h>
#include <libavutil/frame.h>
#include <libavutil/samplefmt.h>
}

namespace exo {

struct LavcDecodeParams {
    bool applyReplayGain;
    double replayGainPreamp;
    bool replayGainAntipeak;
    bool r128Fix;
    bool normalizeVorbisComment;
};

#if !EXO_USE_LIBAVFILTER
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
    /** Accept new values. Newer values take priority over older ones. */
    void accept() noexcept;

    /** Record a ReplayGain gain value. */
    void replayGain(float value) noexcept;

    /** Record a ReplayGain peak value. */
    void replayGainPeak(float value) noexcept;

    /** Record a EBU R128 gain value. */
    void r128Gain(float value) noexcept;

    /** Compute the final volume.
        Returns an empty value if no ReplayGain or R128 data was given. */
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

#if EXO_USE_LIBAVFILTER

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

#else /* EXO_USE_LIBAVFILTER */

struct LavSwrContext : public PointerSlot<LavSwrContext, SwrContext> {
    using PointerSlot::PointerSlot;
    LavSwrContext(SwrContext* p);
    ~LavSwrContext() noexcept;
    EXO_DEFAULT_NONCOPYABLE(LavSwrContext)
};

#endif /* EXO_USE_LIBAVFILTER */

class LavcDecodeJob : public exo::BaseDecodeJob {
    std::string filePath_;
    exo::LavcDecodeParams params_;
    bool canPlay_{false};
    exo::LavPacket packet_;
    exo::LavFrame frame_;
    exo::LavFormatInput formatContext_{nullptr};
    exo::LavCodecContext codecContext_{nullptr};
#if EXO_USE_LIBAVFILTER
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
#if EXO_USE_LIBAVFILTER
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
