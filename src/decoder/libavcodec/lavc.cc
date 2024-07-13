/***
exocaster -- audio streaming helper
decoder/libavcodec/lavc.cc -- libavcodec powered decoder

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

#include <algorithm>
#include <cerrno>
#include <climits>
#include <cmath>
#include <concepts>
#include <cstring>
#include <exception>
#include <new>
#include <span>
#include <sstream>
#include <stdexcept>
#include <utility>

#if !EXO_USE_LIBAVFILTER
#include <cstdio>
#else
#include <limits>
#include <locale>
#include <type_traits>
#endif

#include "config.hh"
#include "decoder/libavcodec/lavc.hh"
#include "log.hh"
#include "metadata.hh"
#include "pcmbuffer.hh"
#include "pcmtypes.hh"
#include "server.hh"
#include "util.hh"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavcodec/codec.h>
#include <libavcodec/codec_id.h>
#include <libavcodec/packet.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/avutil.h>
#include <libavutil/dict.h>
#include <libavutil/error.h>
#include <libavutil/mem.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libavutil/samplefmt.h>
#if EXO_USE_LIBAVFILTER
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#else
#include <libswresample/swresample.h>
#endif
#include <libswscale/swscale.h>
}

#ifndef EXO_LAVC_DEBUG
#if NDEBUG
#define EXO_LAVC_DEBUG 0
#else
#define EXO_LAVC_DEBUG 1
#endif
#endif

namespace exo {

LavPacket::LavPacket() : PointerSlot(av_packet_alloc()) {
    if (!has())
        throw std::bad_alloc();
}

LavPacket::~LavPacket() noexcept {
    if (has()) {
        auto p = release();
        av_packet_free(&p);
    }
}

LavFrame::LavFrame() : PointerSlot(av_frame_alloc()) {
    if (!has())
        throw std::bad_alloc();
}

LavFrame::~LavFrame() noexcept {
    if (has()) {
        auto p = release();
        av_frame_free(&p);
    }
}

LavFormatInput::LavFormatInput(AVFormatContext* p) : PointerSlot(p) {
    if (!has())
        throw std::bad_alloc();
}

LavFormatInput::~LavFormatInput() noexcept {
    if (has()) {
        auto p = release();
        avformat_close_input(&p);
    }
}

LavCodecContext::LavCodecContext(const AVCodec* codec)
    : PointerSlot(avcodec_alloc_context3(codec)) {
    if (!has())
        throw std::bad_alloc();
}

LavCodecContext::~LavCodecContext() noexcept {
    if (has()) {
        auto p = release();
        avcodec_free_context(&p);
    }
}

#if EXO_USE_LIBAVFILTER

LavFilterContext::LavFilterContext(AVFilterContext* p) : PointerSlot(p) {
    if (!has())
        throw std::bad_alloc();
}

LavFilterContext::~LavFilterContext() noexcept {
    if (has())
        avfilter_free(release());
}

LavFilterGraph::LavFilterGraph() : PointerSlot(avfilter_graph_alloc()) {
    if (!has())
        throw std::bad_alloc();
}

LavFilterGraph::~LavFilterGraph() noexcept {
    if (has()) {
        auto p = release();
        avfilter_graph_free(&p);
    }
}

#else /* EXO_USE_LIBAVFILTER */

LavSwrContext::LavSwrContext(SwrContext* p) : PointerSlot(p) {
    if (!has())
        throw std::bad_alloc();
}
LavSwrContext::~LavSwrContext() noexcept {
    if (has()) {
        auto p = release();
        swr_close(p);
        swr_free(&p);
    }
}

#endif /* EXO_USE_LIBAVFILTER */

LavcDecoder::LavcDecoder(const exo::ConfigObject& config,
                         exo::PcmFormat pcmFormat)
    : BaseDecoder(pcmFormat) {
    params_.applyReplayGain =
        cfg::namedBoolean(config, "applyReplayGain", false);
    params_.replayGainPreamp = std::clamp(
        cfg::namedFloat(config, "replayGainPreamp", 0.0), -192.0, 192.0);
    params_.replayGainAntipeak =
        cfg::namedBoolean(config, "replayGainAntipeak", true);
    params_.r128Fix = cfg::namedBoolean(config, "r128Fix", false);
    params_.normalizeVorbisComment =
        cfg::namedBoolean(config, "normalizeVorbisComment", true);
    params_.metadataBlockPicture =
        cfg::namedBoolean(config, "metadataBlockPicture", false);
    params_.metadataBlockPictureMaxSize =
        cfg::namedUInt<unsigned>(config, "metadataBlockPictureMaxSize", 256);
}

std::optional<std::unique_ptr<BaseDecodeJob>>
LavcDecoder::createJob(const exo::ConfigObject& request,
                       std::shared_ptr<exo::ConfigObject> command) {
    if (!cfg::isString(request) && !cfg::isObject(request)) {
        EXO_LOG("lavc decoder: "
                "config not a string or object, ignoring.");
        return {};
    }

    std::string filePath;
    if (cfg::isObject(request)) {
        if (!cfg::hasString(request, "file")) {
            EXO_LOG("lavc decoder: "
                    "request object does not have 'file', ignoring.");
            return {};
        }
        filePath = cfg::namedString(request, "file");
    } else {
        filePath = cfg::getString(request);
    }

    return {std::make_unique<exo::LavcDecodeJob>(
        sink_, pcmFormat_, std::move(command), filePath, params_)};
}

LavcDecodeJob::LavcDecodeJob(std::shared_ptr<exo::PcmSplitter> sink,
                             exo::PcmFormat pcmFormat,
                             std::shared_ptr<exo::ConfigObject> command,
                             const std::string& filePath,
                             const exo::LavcDecodeParams& params)
    : BaseDecodeJob(sink, pcmFormat, command), filePath_(filePath),
      params_(params) {
    decltype(AV_CH_LAYOUT_MONO) formatChannels;

    switch (pcmFormat_.channels) {
    case exo::PcmChannelLayout::Mono:
        formatChannels = AV_CH_LAYOUT_MONO;
        break;
    case exo::PcmChannelLayout::Stereo:
        formatChannels = AV_CH_LAYOUT_STEREO;
        break;
    default:
        throw std::runtime_error("unsupported channel layout");
    }

    if (av_channel_layout_from_mask(&outChLayout_, formatChannels))
        throw std::runtime_error("av_channel_layout_from_mask failed");

    if (pcmFormat_.rate > INT_MAX)
        throw std::runtime_error("unsupported sample rate");

    switch (pcmFormat_.sample) {
    case exo::PcmSampleFormat::U8:
        outSampleFmt_ = AV_SAMPLE_FMT_U8;
        break;
    case exo::PcmSampleFormat::S16:
        outSampleFmt_ = AV_SAMPLE_FMT_S16;
        break;
    case exo::PcmSampleFormat::F32:
        outSampleFmt_ = AV_SAMPLE_FMT_FLT;
        break;
    case exo::PcmSampleFormat::S8:
#ifdef AV_SAMPLE_FMT_S8
        outSampleFmt_ = AV_SAMPLE_FMT_S8;
        break;
#else
        [[fallthrough]];
#endif
    default:
        throw std::runtime_error("lavc decoder: "
                                 "unsupported PCM sample format");
    }
}

static void lavcError_(const char* file, std::size_t lineno, const char* fn,
                       int ret) {
    char errbuf[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(ret, errbuf, sizeof(errbuf));
    exo::log(file, lineno, "%s failed (%d): %s", fn, ret, errbuf);
}

#define EXO_LAVC_ERROR(fn, ret) exo::lavcError_(__FILE__, __LINE__, fn, ret)

#define EXO_LOG_LAV_VERSION(name, prefix, realtime)                            \
    do {                                                                       \
        auto ver_ = realtime();                                                \
        exo::log(__FILE__, __LINE__, "%-14s %2u.%3u.%3u / %2u.%3u.%3u", name,  \
                 prefix##_VERSION_MAJOR, prefix##_VERSION_MINOR,               \
                 prefix##_VERSION_MICRO, (ver_ >> 16), (ver_ >> 8) & 0xFFu,    \
                 (ver_) & 0xFFu);                                              \
    } while (0);

void LavcDecodeJob::init() {
    int ret;

#if EXO_LAVC_DEBUG
    EXO_LOG("dumping libav* versions");
    EXO_LOG_LAV_VERSION("libavutil", LIBAVUTIL, avutil_version);
    EXO_LOG_LAV_VERSION("libavcodec", LIBAVCODEC, avcodec_version);
    EXO_LOG_LAV_VERSION("libavformat", LIBAVFORMAT, avformat_version);
#if EXO_USE_LIBAVFILTER
    EXO_LOG_LAV_VERSION("libavfilter", LIBAVUTIL, avfilter_version);
#endif
    EXO_LOG_LAV_VERSION("libswscale", LIBSWSCALE, swscale_version);
#if !EXO_USE_LIBAVFILTER
    EXO_LOG_LAV_VERSION("libswresample", LIBSWRESAMPLE, swresample_version);
#endif
#endif

    codecContext_.reset();
    if (EXO_UNLIKELY(
            (ret = avformat_open_input(&formatContext_.set(), filePath_.c_str(),
                                       nullptr, nullptr)) < 0 ||
            !formatContext_.get())) {
        EXO_LAVC_ERROR("avformat_open_input", ret);
        return;
    }

    auto formatContext = formatContext_.get();
    if (EXO_UNLIKELY((ret = avformat_find_stream_info(formatContext, nullptr)) <
                     0)) {
        EXO_LAVC_ERROR("avformat_find_stream_info", ret);
        return;
    }

    const AVCodec* codec = nullptr;
    int streamIndex = av_find_best_stream(formatContext, AVMEDIA_TYPE_AUDIO, -1,
                                          -1, &codec, 0);
    if (EXO_UNLIKELY(streamIndex < 0)) {
        EXO_LAVC_ERROR("av_find_best_stream", ret);
        return;
    }
    streamIndex_ = streamIndex;

    // read metadata from the container...
    readMetadata_(formatContext_->metadata);

    // ...and the codec
    readMetadata_(formatContext_->streams[streamIndex]->metadata);

    if (params_.metadataBlockPicture) {
        scanForAlbumArt_();
    }

    // TODO: we do not read metadata from later on in the stream right now.
    // should we? it's probably not relevant in most cases

#if EXO_LAVC_DEBUG
    EXO_LOG("av_dump_format");
    av_dump_format(formatContext, streamIndex, filePath_.c_str(), 0);
#endif

    if (EXO_UNLIKELY(!codec)) {
        EXO_LOG("lavc file codec not supported");
        return;
    }

    try {
        codecContext_ = LavCodecContext(codec);
    } catch (const std::exception& e) {
        EXO_LOG("avcodec_alloc_context3 failed");
        return;
    }
    auto codecContext = codecContext_.get();

    auto codecpar = formatContext_->streams[streamIndex]->codecpar;
    if (EXO_UNLIKELY((ret = avcodec_parameters_to_context(codecContext,
                                                          codecpar)) < 0)) {
        EXO_LAVC_ERROR("avcodec_parameters_to_context", ret);
        return;
    }

    if (EXO_UNLIKELY((ret = avcodec_open2(codecContext, codec, nullptr)) < 0)) {
        EXO_LAVC_ERROR("avcodec_open2", ret);
        return;
    }

    if (codecContext->ch_layout.order == AV_CHANNEL_ORDER_UNSPEC &&
        codecContext->ch_layout.nb_channels <= 2) {
        EXO_LOG("applying av_channel_layout_default to audio stream "
                "with unspecified channel layout");
        av_channel_layout_default(&codecContext->ch_layout,
                                  codecContext->ch_layout.nb_channels);
    }

#if EXO_USE_LIBAVFILTER
    if (!setupFilter_())
        return;
#else
    if (params_.applyReplayGain)
        calculateGain_();

    SwrContext* swrContext = nullptr;
    if (EXO_UNLIKELY(
            (ret = swr_alloc_set_opts2(
                 &swrContext, &outChLayout_, outSampleFmt_, pcmFormat_.rate,
                 &codecContext->ch_layout, codecContext->sample_fmt,
                 codecContext->sample_rate, 0, nullptr)) < 0)) {
        EXO_LAVC_ERROR("swr_alloc_set_opts2", ret);
        return;
    }
    resamplerContext_.reset(swrContext);

    if (EXO_UNLIKELY((ret = swr_init(swrContext)) < 0)) {
        EXO_LAVC_ERROR("swr_init", ret);
        return;
    }
#endif

    canPlay_ = true;
}

template <std::floating_point T> static constexpr T convertR128ToRG(T r128) {
    // EBU R128 is 5 dB lower from ReplayGain
    return r128 + 5.0;
}

static bool alwaysApplyR128Fix(AVCodecContext* codec) {
    // Opus has gain normalized to EBU R128
    return codec->codec_id == AV_CODEC_ID_OPUS;
}

#if EXO_USE_LIBAVFILTER

struct LavcAVFilterInOut : PointerSlot<LavcAVFilterInOut, AVFilterInOut> {
    LavcAVFilterInOut() : PointerSlot(avfilter_inout_alloc()) {}
    ~LavcAVFilterInOut() noexcept {
        if (has()) {
            auto p = release();
            avfilter_inout_free(&p);
        }
    }
    EXO_DEFAULT_NONCOPYABLE(LavcAVFilterInOut);
};

bool LavcDecodeJob::setupFilter_() {
    const AVFilter* abuffer = avfilter_get_by_name("abuffer");
    if (!abuffer) {
        EXO_LOG("lavc: failed to find filter 'abuffer'");
        return false;
    }

    const AVFilter* abuffersink = avfilter_get_by_name("abuffersink");
    if (!abuffersink) {
        EXO_LOG("lavc: failed to find filter 'abuffersink'");
        return false;
    }

    try {
        filterGraph_ = LavFilterGraph();
    } catch (const std::exception& e) {
        EXO_LOG("lavc: failed to allocate filter graph");
        return false;
    }
    auto filterGraph = filterGraph_.get();

    LavcAVFilterInOut outputsSlot, inputsSlot;
    auto outputs = outputsSlot.get(), inputs = inputsSlot.get();

    if (!outputs || !inputs) {
        EXO_LOG("lavc: failed to allocate AVFilterInOut");
        return false;
    }

    char channelDescription[256];

    int ret;
    {
        // build description of input PCM format for abuffer
        auto timeBase = formatContext_->streams[streamIndex_]->time_base;
        const char* sampleFmtName =
            av_get_sample_fmt_name(codecContext_->sample_fmt);
        av_channel_layout_describe(&codecContext_->ch_layout,
                                   channelDescription,
                                   sizeof(channelDescription));
        std::ostringstream pcmDescriptionStream;
        pcmDescriptionStream.imbue(std::locale::classic());
        pcmDescriptionStream
            << "time_base=" << timeBase.num << "/" << timeBase.den << ":"
            << "sample_rate=" << codecContext_->sample_rate << ":"
            << "sample_fmt=" << sampleFmtName << ":"
            << "channel_layout=" << channelDescription;

        auto abufferParam = pcmDescriptionStream.str();
        ret = avfilter_graph_create_filter(&bufferSourceContext_, abuffer, "in",
                                           abufferParam.c_str(), nullptr,
                                           filterGraph);
        if (ret < 0) {
            EXO_LAVC_ERROR("avfilter_graph_create_filter(in)", ret);
            return false;
        }
    }

    ret = avfilter_graph_create_filter(&filterSinkContext_, abuffersink, "out",
                                       nullptr, nullptr, filterGraph);
    if (ret < 0) {
        EXO_LAVC_ERROR("avfilter_graph_create_filter(out)", ret);
        return false;
    }

    // describe output format (PCM buffer format) to abuffersink
    const int outSampleFmts_[] = {outSampleFmt_, -1};
    ret = av_opt_set_int_list(filterSinkContext_, "sample_fmts", outSampleFmts_,
                              -1, AV_OPT_SEARCH_CHILDREN);
    if (ret < 0) {
        EXO_LAVC_ERROR("av_opt_set_int_list(out, format)", ret);
        return false;
    }

    const int outSampleRates_[] = {static_cast<int>(pcmFormat_.rate), -1};
    ret = av_opt_set_int_list(filterSinkContext_, "sample_rates",
                              outSampleRates_, -1, AV_OPT_SEARCH_CHILDREN);
    if (ret < 0) {
        EXO_LAVC_ERROR("av_opt_set_int_list(out, rate)", ret);
        return false;
    }

    av_channel_layout_describe(&outChLayout_, channelDescription,
                               sizeof(channelDescription));
    ret = av_opt_set(filterSinkContext_, "ch_layouts", channelDescription,
                     AV_OPT_SEARCH_CHILDREN);
    if (ret < 0) {
        EXO_LAVC_ERROR("av_opt_set_int_list(out, channels)", ret);
        return false;
    }

    // build AVFilterInOut objects
    outputs->name = av_strdup(bufferSourceContext_->name);
    outputs->filter_ctx = bufferSourceContext_;
    outputs->pad_idx = 0;
    outputs->next = nullptr;

    inputs->name = av_strdup(filterSinkContext_->name);
    inputs->filter_ctx = filterSinkContext_;
    inputs->pad_idx = 0;
    inputs->next = nullptr;

    if (!outputs->name || !inputs->name) {
        EXO_LOG("could not allocate AVFilterInOut names");
        return false;
    }

    auto codecContext = codecContext_.get();
    {
        // construct volume, aresample and aformat filters
        std::ostringstream filterDescriptionStream;
        filterDescriptionStream.imbue(std::locale::classic());

        if (params_.applyReplayGain) {
            filterDescriptionStream << "volume="
                                       "replaygain=track:"
                                       "replaygain_preamp="
                                    << params_.replayGainPreamp
                                    << ":"
                                       "replaygain_noclip="
                                    << params_.replayGainAntipeak;
            if (params_.r128Fix && ((hasR128Gain_ && !hasReplayGain_) ||
                                    exo::alwaysApplyR128Fix(codecContext))) {
                filterDescriptionStream
                    << ":volume=" << exo::convertR128ToRG(0.0) << "dB";
            }
            filterDescriptionStream << ",";
        }

        filterDescriptionStream << "aresample=" << pcmFormat_.rate
                                << ","
                                   "aformat=sample_fmts="
                                << av_get_sample_fmt_name(outSampleFmt_)
                                << ":"
                                   // still has the description of outChLayout_
                                   "channel_layouts="
                                << channelDescription;

        auto filterDesc = filterDescriptionStream.str();
#if EXO_LAVC_DEBUG
        EXO_LOG("avfilter_graph_parse_ptr    %s", filterDesc.c_str());
#endif
        if ((ret = avfilter_graph_parse_ptr(
                 filterGraph, filterDesc.c_str(), &inputsSlot.modify(),
                 &outputsSlot.modify(), nullptr)) < 0) {
            EXO_LAVC_ERROR("avfilter_graph_parse_ptr", ret);
            return false;
        }
    }

    if ((ret = avfilter_graph_config(filterGraph, nullptr)) < 0) {
        EXO_LAVC_ERROR("avfilter_graph_config", ret);
        return false;
    }

    return true;
}

int LavcDecodeJob::filterFrames_(std::shared_ptr<exo::PcmSplitter>& sink,
                                 bool flush) {
    int ret;
    auto filterFrame = filterFrame_.get();

    // pass filtered PCM data from filter sink to main PCM buffer
    while (EXO_LIKELY(exo::shouldRun()) &&
           (ret = av_buffersink_get_frame(filterSinkContext_, filterFrame)) >=
               0) {
        std::size_t dataSize =
            filterFrame_->nb_samples * filterFrame_->ch_layout.nb_channels *
            av_get_bytes_per_sample(
                static_cast<AVSampleFormat>(filterFrame_->format));
        sink->pcm({filterFrame_->data[0], dataSize});
        av_frame_unref(filterFrame);
    }

    if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
        EXO_LAVC_ERROR("av_buffersink_get_frame", ret);
        return ret;
    }
    return 0;
}

#else // !EXO_USE_LIBAVFILTER

// number of fractional bits in LavcGain::i
static constexpr std::size_t REPLAYGAIN_FRAC_BITS = 12;

using GainFixed = decltype(exo::LavcGain::i);
using GainFloat = decltype(exo::LavcGain::f);

template <exo::PcmSampleFormat fmt>
using GainType =
    std::conditional_t<exo::IsSampleFloatingPoint_v<fmt>,
                       decltype(exo::LavcGain::f), decltype(exo::LavcGain::i)>;

template <std::signed_integral T>
T applyGainToSampleSigned_(T sample, exo::GainFixed gain) {
    using Wide = exo::WiderType_t<T>;
    // apply gain through integer (fixed-point) multiplication
    auto x = (static_cast<Wide>(sample) * gain) >> REPLAYGAIN_FRAC_BITS;
    return static_cast<T>(
        std::clamp(x, static_cast<Wide>(std::numeric_limits<T>::min()),
                   static_cast<Wide>(std::numeric_limits<T>::max())));
}

template <std::unsigned_integral T>
std::make_signed_t<T> convertUnsignedToSigned_(T sample) {
    using SignedT = std::make_signed_t<T>;
    using Wide = exo::WiderType_t<T>;
    return static_cast<SignedT>(
        static_cast<Wide>(sample) +
        static_cast<Wide>(std::numeric_limits<SignedT>::min()));
}

template <std::signed_integral T>
std::make_unsigned_t<T> convertSignedToUnsigned_(T sample) {
    using UnsignedT = std::make_unsigned_t<T>;
    using Wide = exo::WiderType_t<T>;
    return static_cast<UnsignedT>(
        static_cast<Wide>(sample) -
        static_cast<Wide>(std::numeric_limits<T>::min()));
}

template <std::unsigned_integral T>
T applyGainToSampleUnsigned_(T sample, exo::GainFixed gain) {
    return exo::convertSignedToUnsigned_(exo::applyGainToSampleSigned_(
        exo::convertUnsignedToSigned_(sample), gain));
}

template <std::floating_point T>
T applyGainToSampleFloat_(T sample, exo::GainFloat gain) {
    // apply gain through floating-point multiplication
    return std::clamp(static_cast<T>(sample * gain), T{-1}, T{1});
}

template <exo::PcmSampleFormat fmt> struct false_ : std::false_type {};

template <exo::PcmSampleFormat fmt>
static exo::PcmFormat_t<fmt> applyGainToSample_(exo::PcmFormat_t<fmt> sample,
                                                exo::GainType<fmt> gain) {
    if constexpr (std::is_floating_point_v<exo::PcmFormat_t<fmt>>)
        return applyGainToSampleFloat_(sample, gain);
    else if constexpr (std::is_signed_v<exo::PcmFormat_t<fmt>>)
        return applyGainToSampleSigned_(sample, gain);
    else if constexpr (std::is_unsigned_v<exo::PcmFormat_t<fmt>>)
        return applyGainToSampleUnsigned_(sample, gain);
    else
        static_assert(false_<fmt>::value);
}

template <exo::PcmSampleFormat fmt>
static void applyReplayGain_(void* buffer, std::size_t samples,
                             exo::GainType<fmt> gain) {
    auto ptr = reinterpret_cast<exo::PcmFormat_t<fmt>*>(buffer);
    for (std::size_t i = 0; i < samples; ++i)
        ptr[i] = exo::applyGainToSample_<fmt>(ptr[i], gain);
}

template <exo::PcmSampleFormat fmt>
static exo::GainType<fmt> getGain(const exo::LavcGain& gain) {
    if constexpr (exo::IsSampleFloatingPoint_v<fmt>) {
        return gain.f;
    } else {
        return gain.i;
    }
}

static void applyReplayGain(exo::PcmFormat pcmfmt, void* dst,
                            std::size_t frames, exo::LavcGain gain) {
    auto channels = exo::channelCount(pcmfmt.channels);
    switch (pcmfmt.sample) {
#define EXO_PCM_FORMATS_CASE(F)                                                \
    case exo::PcmSampleFormat::F:                                              \
        return exo::applyReplayGain_<exo::PcmSampleFormat::F>(                 \
            dst, frames * channels,                                            \
            exo::getGain<exo::PcmSampleFormat::F>(gain));
        EXO_PCM_FORMATS_SWITCH
#undef EXO_PCM_FORMATS_CASE
    default:
        EXO_UNREACHABLE;
    }
}

static constexpr unsigned short MASK_REPLAYGAIN = 1;
static constexpr unsigned short MASK_REPLAYGAIN_PEAK = 2;
static constexpr unsigned short MASK_R128GAIN = 4;

bool LavcGainCalculator::has_(unsigned short mask) const noexcept {
    return static_cast<bool>(hasMask_ & mask);
}

bool LavcGainCalculator::accepts_(unsigned short mask) noexcept {
    if (rejectMask_ & mask)
        return false;
    hasMask_ |= mask;
    rejectMask_ |= mask;
    return true;
}

void LavcGainCalculator::accept() noexcept { rejectMask_ = 0; }

void LavcGainCalculator::replayGain(float value) noexcept {
    if (accepts_(MASK_REPLAYGAIN))
        replayGain_ = value;
}

void LavcGainCalculator::replayGainPeak(float value) noexcept {
    if (accepts_(MASK_REPLAYGAIN_PEAK))
        replayGainPeak_ = value;
}

void LavcGainCalculator::r128Gain(float value) noexcept {
    if (accepts_(MASK_R128GAIN)) {
        r128Gain_ = value;

        /* if we are still accepting ReplayGain, then we could only have
           ReplayGain data from a lower priority source, so discard it */
        if (!(rejectMask_ & MASK_REPLAYGAIN))
            hasMask_ &= ~MASK_REPLAYGAIN;
    }
}

template <typename T>
    requires(std::is_arithmetic_v<T>)
static T exp10(T value) {
    return std::pow(T{10}, value);
}

std::optional<double> LavcGainCalculator::gain(bool antipeak,
                                               double preamp) const noexcept {
    double rg;
    if (has_(MASK_REPLAYGAIN)) {
        rg = replayGain_;
    } else if (has_(MASK_R128GAIN)) {
        rg = exo::convertR128ToRG(r128Gain_);
    } else {
#if EXO_LAVC_DEBUG
        EXO_LOG("no ReplayGain found, not applying");
#endif
        return {};
    }

#if EXO_LAVC_DEBUG
    EXO_LOG("detected ReplayGain %+.2f dB", rg);
#endif

    // compute final gain factor to apply
    double volume = exo::exp10((rg + preamp) * 0.05);

    // apply peak prevention
    if (antipeak && has_(MASK_REPLAYGAIN) && has_(MASK_REPLAYGAIN_PEAK) &&
        replayGainPeak_ > 0)
        volume = std::min(volume, 1.0 / replayGainPeak_);

    return volume;
}

void LavcDecodeJob::calculateGain_() {
    if (!params_.applyReplayGain)
        return;

    auto gain = gainCalculator_.gain(params_.replayGainAntipeak,
                                     params_.replayGainPreamp);
    if (gain.has_value()) {
        auto volume = gain.value();
        if (exo::areSamplesFloatingPoint(pcmFormat_.sample)) {
            gain_.f = volume;
        } else {
            gain_.i = static_cast<decltype(gain_.i)>(
                0.5 + volume * (static_cast<decltype(gain_.i)>(1)
                                << REPLAYGAIN_FRAC_BITS));
        }
    } else {
        params_.applyReplayGain = false;
    }
}

int LavcDecodeJob::processResampledFrame_(
    std::shared_ptr<exo::PcmSplitter>& sink, const AVFrame* frame) {
    int size = av_samples_get_buffer_size(nullptr, outChLayout_.nb_channels,
                                          frame->nb_samples, outSampleFmt_, 1);
    if (EXO_UNLIKELY(size < 0)) {
        EXO_LAVC_ERROR("av_samples_get_buffer_size", size);
        return size;
    }

    exo::byte* buffer = frame->data[0];
    std::size_t frameCount = static_cast<std::size_t>(frame->nb_samples);
    if (params_.applyReplayGain)
        exo::applyReplayGain(pcmFormat_, buffer, frameCount, gain_);
    sink->pcm({buffer, static_cast<std::size_t>(size)});
    return 0;
}

int LavcDecodeJob::processFrame_(std::shared_ptr<exo::PcmSplitter>& sink,
                                 const AVFrame* frame) {
    auto resamplerContext = resamplerContext_.get();
    auto resamplerFrame = resamplerFrame_.get();

    resamplerFrame_->ch_layout = outChLayout_;
    resamplerFrame_->sample_rate = pcmFormat_.rate;
    resamplerFrame_->format = outSampleFmt_;

    int err = swr_convert_frame(resamplerContext, resamplerFrame, frame);
    if (EXO_UNLIKELY(err < 0)) {
        EXO_LAVC_ERROR("swr_convert_frame", err);
        return err;
    }

    err = processResampledFrame_(sink, resamplerFrame);
    av_frame_unref(resamplerFrame);
    if (err < 0)
        return err;
    return 0;
}
#endif // EXO_USE_LIBAVFILTER

int LavcDecodeJob::decodeFrames_(std::shared_ptr<exo::PcmSplitter>& sink,
                                 bool flush) {
    int ret;
    auto codecContext = codecContext_.get();
    auto frame = frame_.get();

    while ((ret = avcodec_receive_frame(codecContext, frame)) >= 0 &&
           EXO_LIKELY(exo::shouldRun())) {
#if EXO_USE_LIBAVFILTER
        if (EXO_UNLIKELY((ret = av_buffersrc_add_frame_flags(
                              bufferSourceContext_, frame,
                              flush ? (AV_BUFFERSRC_FLAG_PUSH |
                                       AV_BUFFERSRC_FLAG_KEEP_REF)
                                    : AV_BUFFERSRC_FLAG_KEEP_REF)) < 0)) {
            EXO_LAVC_ERROR("av_buffersrc_add_frame_flags", ret);
            av_frame_unref(frame);
            return ret;
        }

        ret = filterFrames_(sink, flush);
#else
        // hack <https://stackoverflow.com/q/77502983>
        if (frame->ch_layout.order == AV_CHANNEL_ORDER_UNSPEC &&
            frame->ch_layout.nb_channels <= 2) {
            av_channel_layout_default(&frame->ch_layout,
                                      frame->ch_layout.nb_channels);
        }

        ret = processFrame_(sink, frame);
#endif

        av_frame_unref(frame);
        if (EXO_UNLIKELY(ret < 0))
            return ret;
    }

    if (EXO_UNLIKELY(ret != AVERROR(EAGAIN) && ret != AVERROR_EOF))
        EXO_LAVC_ERROR("avcodec_receive_frame", ret);
    return ret;
}

void LavcDecodeJob::flush_(std::shared_ptr<exo::PcmSplitter>& sink) {
#if EXO_USE_LIBAVFILTER
    filterFrames_(sink, true);
#else
    processFrame_(sink, nullptr);
#endif
}

/** libavformat changes some of these, change them back to how
    Vorbis files normally name these fields */
static exo::CaseInsensitiveMap<std::string> normalizedVorbisCommentKeys = {
    {"album_artist", "ALBUMARTIST"},
    {"track", "TRACKNUMBER"},
    {"disc", "DISCNUMBER"},
    {"comment", "DESCRIPTION"},
};

void LavcDecodeJob::readMetadata_(const AVDictionary* metadict) {
    const AVDictionaryEntry* tag = nullptr;
#if !EXO_USE_LIBAVFILTER
    if (codecContext_ && alwaysApplyR128Fix(codecContext_.get()))
        gainCalculator_.r128Gain(0.0f);
    gainCalculator_.accept();
#endif

#if LIBAVUTIL_VERSION_MAJOR > 57 ||                                            \
    (LIBAVUTIL_VERSION_MAJOR == 57 && LIBAVUTIL_VERSION_MINOR >= 42)
    while ((tag = av_dict_iterate(metadict, tag)))
#else
    while ((tag = av_dict_get(metadict, "", tag, AV_DICT_IGNORE_SUFFIX)))
#endif
    {
        if (!exo::strnicmp(tag->key, "REPLAYGAIN_", 11)) {
            // do not forward ReplayGain tags
#if EXO_USE_LIBAVFILTER
            hasReplayGain_ = true;
#else
            if (params_.applyReplayGain) {
                // record the gain if we find it
                if (!exo::stricmp(tag->key, "REPLAYGAIN_TRACK_GAIN")) {
                    double gain;
                    if (std::sscanf(tag->value, " %lf [dD][bB]", &gain) > 0) {
                        gainCalculator_.replayGain(static_cast<float>(gain));
                    }
                } else if (!exo::stricmp(tag->key, "REPLAYGAIN_TRACK_PEAK")) {
                    double peak;
                    if (std::sscanf(tag->value, " %lf", &peak) > 0) {
                        gainCalculator_.replayGainPeak(
                            static_cast<float>(peak));
                    }
                }
            }
#endif
        } else if (!exo::stricmp(tag->key, "R128_TRACK_GAIN")) {
#if EXO_USE_LIBAVFILTER
            hasR128Gain_ = true;
#else
            if (params_.applyReplayGain) {
                int gain;
                if (std::sscanf(tag->value, "%d", &gain) > 0) {
                    gainCalculator_.r128Gain(gain / 256.0f);
                }
            }
#endif

        } else if (params_.normalizeVorbisComment) {
            auto it =
                exo::normalizedVorbisCommentKeys.find(std::string(tag->key));
            if (it != exo::normalizedVorbisCommentKeys.end())
                metadata_.push_back({it->second, tag->value});
            else
                metadata_.push_back({tag->key, tag->value});

        } else {
            metadata_.push_back({tag->key, tag->value});
        }
    }
}

void LavcDecodeJob::run(std::shared_ptr<exo::PcmSplitter> sink) {
    if (!canPlay_)
        return;

    sink->metadata(command_, metadata_);
    int ret;

    auto formatContext = formatContext_.get();
    auto codecContext = codecContext_.get();
    auto packet = packet_.get();

    while ((ret = av_read_frame(formatContext, packet) >= 0) &&
           EXO_LIKELY(exo::shouldRun())) {
        if (packet_->stream_index == streamIndex_) {
            if (EXO_UNLIKELY((ret = avcodec_send_packet(codecContext, packet)) <
                             0)) {
                EXO_LAVC_ERROR("avcodec_send_packet", ret);
                break;
            }
        }
        av_packet_unref(packet);

        ret = decodeFrames_(sink, false);
        if (EXO_UNLIKELY(ret != AVERROR(EAGAIN)))
            break;
    }

    if (EXO_UNLIKELY(ret < 0 && ret != AVERROR_EOF))
        EXO_LAVC_ERROR("av_read_frame", ret);

    // flush codec
    if (EXO_UNLIKELY((ret = avcodec_send_packet(codecContext, nullptr)) < 0))
        EXO_LAVC_ERROR("avcodec_send_packet", ret);
    decodeFrames_(sink, true);

    // flush resampler
#if EXO_USE_LIBAVFILTER
    if (av_buffersrc_add_frame_flags(bufferSourceContext_, nullptr, 0) < 0)
        EXO_LAVC_ERROR("av_buffersrc_add_frame_flags(EOF)", ret);
#else
    if (EXO_UNLIKELY((ret = swr_convert(resamplerContext_.get(), nullptr, 0,
                                        nullptr, 0)) < 0))
        EXO_LAVC_ERROR("swr_convert(EOF)", ret);
#endif
    flush_(sink);
}

/** Writes an unsigned 32-bit integer into the stream as big-endian. */
template <typename Stream>
static void writeBE32(Stream& stream, std::uint32_t value) {
    stream << static_cast<unsigned char>((value >> 24) & 0xFFU);
    stream << static_cast<unsigned char>((value >> 16) & 0xFFU);
    stream << static_cast<unsigned char>((value >> 8) & 0xFFU);
    stream << static_cast<unsigned char>((value) & 0xFFU);
}

/** Base64 alphabet */
static char base64Chars[64] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
    'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
    'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/'};

template <typename Stream>
static void encodeBase64Group(Stream& stream, const std::array<byte, 3>& b,
                              std::size_t k) {
    std::size_t i;
    std::uint32_t w = 0;
    ++k;
    for (i = 0; i < b.size(); ++i)
        w = (w << 8) | static_cast<decltype(w)>(b[i]);
    for (i = 0; i < k; ++i)
        stream << base64Chars[(w >> 18) & 63], w <<= 6;
    for (i = k; i < 4; ++i)
        stream << '=';
}

/** Encodes the given string of bytes into the stream as Base64. */
template <typename Stream>
static void encodeBase64(Stream& stream, const std::string_view& sv) {
    std::size_t i = 0, n = sv.size();

    while (i < n) {
        std::size_t k = 0;
        std::array<byte, 3> b{0, 0, 0};

        if (i < n)
            b[k++] = sv[i++];
        if (i < n)
            b[k++] = sv[i++];
        if (i < n)
            b[k++] = sv[i++];

        encodeBase64Group(stream, b, k);
    }
}

struct AlbumArtImage {
    const char* mime;
    unsigned width;
    unsigned height;
    unsigned depth;
    LavPacket data;
};

struct SwsContext : PointerSlot<SwsContext, ::SwsContext> {
    using PointerSlot::PointerSlot;
    SwsContext(::SwsContext* p) : PointerSlot(p) {}
    ~SwsContext() noexcept {
        if (has())
            sws_freeContext(release());
    }
    EXO_DEFAULT_NONCOPYABLE(SwsContext);
};

static exo::AlbumArtImage downscaleImage(AVFrame* frame, unsigned targetSize) {
    int ret;
    auto sourceWidth = static_cast<unsigned>(frame->width),
         sourceHeight = static_cast<unsigned>(frame->height);
    auto sourceFormat = static_cast<AVPixelFormat>(frame->format);
    unsigned targetWidth, targetHeight;
    AVPixelFormat targetFormat = AV_PIX_FMT_YUV420P;

    auto pixFmtDescriptor =
        av_pix_fmt_desc_get(static_cast<AVPixelFormat>(targetFormat));
    if (!pixFmtDescriptor)
        throw std::runtime_error("unsupported target pixel format");

    double aspectRatio = static_cast<double>(sourceWidth) / sourceHeight;
    double newWidth, newHeight;
    if (aspectRatio >= 1) {
        newWidth = targetSize;
        newHeight = targetSize / aspectRatio;
    } else {
        newWidth = targetSize * aspectRatio;
        newHeight = targetSize;
    }
    targetWidth = std::max(static_cast<unsigned>(std::round(newWidth)), 1U);
    targetHeight = std::max(static_cast<unsigned>(std::round(newHeight)), 1U);

    SwsContext swsContext(sws_getCachedContext(
        nullptr, frame->width, frame->height, sourceFormat, targetWidth,
        targetHeight, targetFormat, SWS_BICUBIC, nullptr, nullptr, nullptr));
    if (!swsContext.has())
        throw std::bad_alloc();

    int srcRange, dstRange;
    int brightness, contrast, saturation;
    int tables[4];
    if ((ret = sws_getColorspaceDetails(
             swsContext.get(), (int**)&tables, &srcRange, (int**)&tables,
             &dstRange, &brightness, &contrast, &saturation)) < 0)
        throw std::runtime_error("sws_getColorspaceDetails failed");
    const int* coefs = sws_getCoefficients(SWS_CS_DEFAULT);
    srcRange = 1;
    if ((ret = sws_setColorspaceDetails(swsContext.get(), coefs, srcRange,
                                        coefs, dstRange, brightness, contrast,
                                        saturation)) < 0)
        throw std::runtime_error("sws_setColorspaceDetails failed");

    LavFrame targetFrame;
    if ((ret = sws_scale_frame(swsContext.get(), targetFrame.get(), frame)) < 0)
        throw std::runtime_error("sws_scale_frame failed");

    auto jpeg = avcodec_find_encoder(AV_CODEC_ID_MJPEG);
    if (!jpeg)
        throw std::runtime_error("no JPEG encoder found");

    constexpr unsigned Q = 3;
    LavCodecContext jpegContext(jpeg);
    jpegContext->width = targetWidth;
    jpegContext->height = targetHeight;
    jpegContext->pix_fmt = targetFormat;
    jpegContext->flags |= AV_CODEC_FLAG_QSCALE;
    jpegContext->qmin = jpegContext->qmax = Q;
    jpegContext->time_base = AVRational{1, 25};
    jpegContext->codec_id = AV_CODEC_ID_MJPEG;
    jpegContext->color_range = AVCOL_RANGE_JPEG;
    jpegContext->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;
    targetFrame->quality = FF_QP2LAMBDA * Q;

    if ((ret = avcodec_open2(jpegContext.get(), jpeg, nullptr)) < 0)
        throw std::runtime_error("avcodec_open2 JPEG failed");

    if ((ret = avcodec_send_frame(jpegContext.get(), targetFrame.get())) < 0)
        throw std::runtime_error("avcodec_receive_frame JPEG failed");

    targetFrame->width = targetWidth;
    targetFrame->height = targetHeight;
    targetFrame->color_range = AVCOL_RANGE_JPEG;
    targetFrame->quality = FF_QP2LAMBDA * Q;

    LavPacket pkt;
    if ((ret = avcodec_receive_packet(jpegContext.get(), pkt.get())) < 0)
        throw std::runtime_error("avcodec_receive_frame JPEG failed");

    return {.mime = "image/jpeg",
            .width = targetWidth,
            .height = targetHeight,
            .depth =
                static_cast<unsigned>(av_get_bits_per_pixel(pixFmtDescriptor)),
            .data = std::move(pkt)};
}

// 0x0 image with MIME type "", description "", type 0 (Other)
static std::string emptyMetadataBlockPicture =
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=";

void LavcDecodeJob::scanForAlbumArt_() {
    // find stream with AV_DISPOSITION_ATTACHED_PIC
    AVStream* picStream = nullptr;
    for (unsigned i = 0; i < formatContext_->nb_streams; ++i) {
        if (formatContext_->streams[i]->disposition &
            AV_DISPOSITION_ATTACHED_PIC) {
            picStream = formatContext_->streams[i];
            break;
        }
    }
    if (!picStream) {
        metadata_.push_back({
            "METADATA_BLOCK_PICTURE",
            exo::emptyMetadataBlockPicture,
        });
        return;
    }

    auto attachedPic = picStream->attached_pic;
    const char* mime;
    // check codec
    switch (picStream->codecpar->codec_id) {
    case AV_CODEC_ID_MJPEG:
        mime = "image/jpeg";
        break;
    case AV_CODEC_ID_PNG:
        mime = "image/png";
        break;
    default:
        metadata_.push_back(
            {"METADATA_BLOCK_PICTURE", exo::emptyMetadataBlockPicture});
        return;
    }

    if (attachedPic.size < 0 ||
        static_cast<unsigned>(attachedPic.size) > UINT32_MAX)
        return;
    std::uint32_t attachedPicSize = attachedPic.size;

    const char* description = "Cover (front)";

    try {
        // decode frame to get width, height and pixel format
        int ret;
        auto codec = avcodec_find_decoder(picStream->codecpar->codec_id);
        if (!codec)
            return;

        LavCodecContext codecContextHolder(codec);
        auto codecContext = codecContextHolder.get();

        auto codecpar = picStream->codecpar;
        if ((ret = avcodec_parameters_to_context(codecContext, codecpar)) < 0)
            return;

        if (EXO_UNLIKELY((ret = avcodec_open2(codecContext, codec, nullptr)) <
                         0))
            return;

        LavFrame frame;
        avcodec_send_packet(codecContext, &attachedPic);
        if (avcodec_receive_frame(codecContext, frame.get()))
            return;

        auto pixFmtDescriptor =
            av_pix_fmt_desc_get(static_cast<AVPixelFormat>(frame->format));
        if (!pixFmtDescriptor)
            return;

        unsigned width = frame->width;
        unsigned height = frame->height;
        unsigned colorDepthBits = av_get_bits_per_pixel(pixFmtDescriptor);
        std::span<const char> data = {
            reinterpret_cast<const char*>(attachedPic.data), attachedPicSize};
        LavPacket imageDataBuffer;

        if (!width || !height)
            return;

        if (std::max(width, height) > params_.metadataBlockPictureMaxSize ||
            data.size() > width * height * colorDepthBits / CHAR_BIT) {
            try {
                exo::AlbumArtImage img = exo::downscaleImage(
                    frame.get(), params_.metadataBlockPictureMaxSize);
                mime = img.mime;
                width = img.width;
                height = img.height;
                colorDepthBits = img.depth;
                imageDataBuffer = std::move(img.data);
                data = {reinterpret_cast<const char*>(imageDataBuffer->data),
                        static_cast<std::size_t>(imageDataBuffer->size)};
            } catch (const std::runtime_error& e) {
                EXO_LOG("could not downscale image for album art: %s",
                        e.what());
                return;
            }
#if EXO_LAVC_DEBUG
            EXO_LOG(
                "downscaled album art to %ux%u, shrunk from %.1f KB to %.1f KB",
                width, height, attachedPicSize * 1.0e-3, data.size() * 1.0e-3);
#endif
        }

        // generate FLAC METADATA_BLOCK_PICTURE block
        std::ostringstream binaryPicStream;
        exo::writeBE32(binaryPicStream, 3); // Cover (front)
        exo::writeBE32(binaryPicStream, std::strlen(mime));
        binaryPicStream << mime;
        exo::writeBE32(binaryPicStream, std::strlen(description));
        binaryPicStream << description;
        exo::writeBE32(binaryPicStream, width);          // width
        exo::writeBE32(binaryPicStream, height);         // height
        exo::writeBE32(binaryPicStream, colorDepthBits); // color depth
        exo::writeBE32(binaryPicStream, 0);              // not indexed
        exo::writeBE32(binaryPicStream, data.size());
        binaryPicStream.write(data.data(), data.size());

        auto binaryPic = std::move(binaryPicStream).str();
        std::ostringstream base64PicStream;
        exo::encodeBase64(base64PicStream, binaryPic);
        metadata_.push_back(
            {"METADATA_BLOCK_PICTURE", std::move(base64PicStream).str()});

    } catch (const std::bad_alloc&) {
        return;
    }
}

} // namespace exo
