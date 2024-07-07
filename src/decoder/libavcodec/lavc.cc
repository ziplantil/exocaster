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

#include <cmath>
#if !USE_LIBAVFILTER
#include <cstdio>
#else
#include <sstream>
#include <type_traits>
#endif

#include "decoder/libavcodec/lavc.hh"
#include "config.hh"
#include "log.hh"
#include "metadata.hh"
#include "pcmtypes.hh"
#include "util.hh"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavcodec/codec_id.h>
#include <libavcodec/packet.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
#if USE_LIBAVFILTER
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#endif
}

#ifndef EXO_LAVC_DEBUG
#if NDEBUG
#define EXO_LAVC_DEBUG 0
#else
#define EXO_LAVC_DEBUG 1
#endif
#endif

namespace exo {

LavcDecoder::LavcDecoder(const exo::ConfigObject& config,
                exo::PcmFormat pcmFormat) : BaseDecoder(pcmFormat) {
    params_.applyReplayGain = cfg::namedBoolean(config,
                            "applyReplayGain", false);
    params_.replayGainPreamp = std::clamp(cfg::namedFloat(config,
                            "replayGainPreamp", 0.0),
                                    -192.0, 192.0);
    params_.replayGainAntipeak = cfg::namedBoolean(config,
                            "replayGainAntipeak", true);
    params_.r128Fix = cfg::namedBoolean(config, "r128Fix", false);
    params_.normalizeVorbisComment = cfg::namedBoolean(config,
                "normalizeVorbisComment", true);
}

std::optional<std::unique_ptr<BaseDecodeJob>> LavcDecoder::createJob(
            const exo::ConfigObject& request,
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

    return {
        std::make_unique<exo::LavcDecodeJob>(sink_, pcmFormat_,
                std::move(command), filePath, params_)
    };
}

LavcDecodeJob::LavcDecodeJob(std::shared_ptr<exo::PcmSplitter> sink,
                             exo::PcmFormat pcmFormat,
                             std::shared_ptr<exo::ConfigObject> command,
                             const std::string& filePath,
                             const exo::LavcDecodeParams& params)
    : BaseDecodeJob(sink, pcmFormat, command),
      filePath_(filePath), params_(params) {
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

    if (pcmFormat_.rate > INT_MAX)
        throw std::runtime_error("unsupported sample rate");

    if (av_channel_layout_from_mask(&outChLayout_, formatChannels))
        throw std::runtime_error("av_channel_layout_from_mask failed");

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

    if (!(packet_ = av_packet_alloc())) throw std::bad_alloc();
    if (!(frame_ = av_frame_alloc())) throw std::bad_alloc();
#if USE_LIBAVFILTER
    if (!(filterFrame_ = av_frame_alloc())) throw std::bad_alloc();
#else
    int ret;
    bufferFrameCount_ = pcmFormat_.rate / 4;
    ret = av_samples_alloc(&buffer_, nullptr, outChLayout_.nb_channels,
                           bufferFrameCount_, outSampleFmt_,
                           1);
    if (ret < 0 || !buffer_) throw std::bad_alloc();
#endif
}

static void lavcError_(const char* file, std::size_t lineno,
                       const char* fn, int ret) {
    char errbuf[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(ret, errbuf, sizeof(errbuf));
    exo::log(file, lineno, "%s failed (%d): %s", fn, ret, errbuf);
}

#define EXO_LAVC_ERROR(fn, ret) exo::lavcError_(__FILE__, __LINE__, fn, ret)

void LavcDecodeJob::init() {
    int ret;

    if (EXO_UNLIKELY((ret = avformat_open_input(&formatContext_,
                    filePath_.c_str(), nullptr, nullptr)) < 0)) {
        EXO_LAVC_ERROR("avformat_open_input", ret);
        return;
    }

    if (EXO_UNLIKELY((ret = avformat_find_stream_info(
                            formatContext_, nullptr)) < 0)) {
        EXO_LAVC_ERROR("avformat_find_stream_info", ret);
        return;
    }

    readMetadata_(formatContext_->metadata);

    int streamIndex = av_find_best_stream(formatContext_, AVMEDIA_TYPE_AUDIO,
                                          -1, -1, nullptr, 0);
    if (EXO_UNLIKELY(streamIndex < 0)) {
        EXO_LAVC_ERROR("av_find_best_stream", ret);
        return;
    }
    streamIndex_ = streamIndex;

    readMetadata_(formatContext_->streams[streamIndex]->metadata);

#if EXO_LAVC_DEBUG
    EXO_LOG("av_dump_format");
    av_dump_format(formatContext_, streamIndex, filePath_.c_str(), 0);
#endif

    auto codecpar = formatContext_->streams[streamIndex]->codecpar;
    const AVCodec* codec = avcodec_find_decoder(codecpar->codec_id);
    if (EXO_UNLIKELY(!codec)) {
        EXO_LOG("lavc file codec not supported");
        return;
    }

    codecContext_ = avcodec_alloc_context3(codec);
    if (EXO_UNLIKELY(!codecContext_)) {
        EXO_LOG("avcodec_alloc_context3 failed");
        return;
    }

    if (EXO_UNLIKELY((ret = avcodec_parameters_to_context(
                    codecContext_, codecpar)) < 0)) {
        EXO_LAVC_ERROR("avcodec_parameters_to_context", ret);
        return;
    }

    if (EXO_UNLIKELY((ret = avcodec_open2(codecContext_,
                                codec, nullptr)) < 0)) {
        EXO_LAVC_ERROR("avcodec_open2", ret);
        return;
    }

    if (codecContext_->ch_layout.order == AV_CHANNEL_ORDER_UNSPEC
            && codecContext_->ch_layout.nb_channels <= 2) {
        EXO_LOG("av_channel_layout_default");
        av_channel_layout_default(&codecContext_->ch_layout,
                codecContext_->ch_layout.nb_channels);
    }

#if USE_LIBAVFILTER
    if (!setupFilter_())
        return;
#else
    if (params_.applyReplayGain)
        calculateGain_();

    if (EXO_UNLIKELY((ret = swr_alloc_set_opts2(&resamplerContext_,
                              &outChLayout_,
                              outSampleFmt_,
                              pcmFormat_.rate,
                              &codecContext_->ch_layout,
                              codecContext_->sample_fmt,
                              codecContext_->sample_rate,
                              0,
                              nullptr)) < 0)) {
        EXO_LAVC_ERROR("swr_alloc_set_opts2", ret);
        return;
    }

    if (EXO_UNLIKELY((ret = swr_init(resamplerContext_)) < 0)) {
        EXO_LAVC_ERROR("swr_init", ret);
        return;
    }
#endif

    canPlay_ = true;
}

int LavcDecodeJob::decodeFrames_(std::shared_ptr<exo::PcmSplitter>& sink,
                                 bool flush) {
    int ret;
    while ((ret = avcodec_receive_frame(codecContext_, frame_)) >= 0) {
#if USE_LIBAVFILTER
        if (EXO_UNLIKELY((ret = av_buffersrc_add_frame_flags(
                            bufferSourceContext_, frame_,
                            flush ? (AV_BUFFERSRC_FLAG_PUSH
                                    | AV_BUFFERSRC_FLAG_KEEP_REF)
                                    : AV_BUFFERSRC_FLAG_KEEP_REF)) < 0)) {
            EXO_LAVC_ERROR("av_buffersrc_add_frame_flags", ret);
            av_frame_unref(frame_);
            return ret;
        }

        ret = filterFrames_(sink, flush);
#else
        // hack <https://stackoverflow.com/q/77502983>
        if (frame_->ch_layout.order == AV_CHANNEL_ORDER_UNSPEC
                && frame_->ch_layout.nb_channels <= 2) {
            av_channel_layout_default(&frame_->ch_layout,
                    frame_->ch_layout.nb_channels);
        }

        ret = processFrame_(sink, frame_);
#endif

        av_frame_unref(frame_);
        if (EXO_UNLIKELY(ret < 0)) return ret;
    }
    if (EXO_UNLIKELY(ret != AVERROR(EAGAIN) && ret != AVERROR_EOF))
        EXO_LAVC_ERROR("avcodec_receive_frame", ret);
    return ret;
}

template <std::floating_point T>
static constexpr T convertR128ToRG(T r128) {
    return r128 + 5.0;
}

#if USE_LIBAVFILTER

static bool alwaysApplyR128Fix(AVCodecContext* codec) {
    // Opus has track gain
    return codec->codec_id == AV_CODEC_ID_OPUS;
}

struct LavcAVFilterInOut {
    AVFilterInOut* ptr{nullptr};

    LavcAVFilterInOut() noexcept {
        ptr = avfilter_inout_alloc();
    }
    ~LavcAVFilterInOut() noexcept {
        // if (ptr) avfilter_inout_free(&ptr);
    }

    EXO_DEFAULT_NONMOVABLE(LavcAVFilterInOut);
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

    filterGraph_ = avfilter_graph_alloc();
    if (!filterGraph_) {
        EXO_LOG("lavc: failed to allocate filter graph");
        return false;
    }

    LavcAVFilterInOut outputsSlot, inputsSlot;
    auto outputs = outputsSlot.ptr, inputs = inputsSlot.ptr;

    if (!outputs || !inputs) {
        EXO_LOG("lavc: failed to allocate av filter I/O");
        return false;
    }

    char channelDescription[256];
    auto timeBase = formatContext_->streams[streamIndex_]->time_base;
    const char* sampleFmtName = av_get_sample_fmt_name(
                codecContext_->sample_fmt);
    av_channel_layout_describe(&codecContext_->ch_layout,
                               channelDescription, sizeof(channelDescription));

    int ret;
    {
        std::ostringstream pcmDescriptionStream;
        pcmDescriptionStream.imbue(std::locale::classic());
        pcmDescriptionStream
                << "time_base=" << timeBase.num << "/" << timeBase.den << ":"
                << "sample_rate=" << codecContext_->sample_rate << ":"
                << "sample_fmt=" << sampleFmtName << ":"
                << "channel_layout=" << channelDescription;

        auto abufferParam = pcmDescriptionStream.str();
        ret = avfilter_graph_create_filter(&bufferSourceContext_, abuffer,
                                           "in", abufferParam.c_str(),
                                           nullptr, filterGraph_);
        if (ret < 0) {
            EXO_LAVC_ERROR("avfilter_graph_create_filter(in)", ret);
            return false;
        }
    }

    ret = avfilter_graph_create_filter(&filterSinkContext_, abuffersink,
                                       "out", nullptr,
                                       nullptr, filterGraph_);
    if (ret < 0) {
        EXO_LAVC_ERROR("avfilter_graph_create_filter(out)", ret);
        return false;
    }

    const int outSampleFmts_[] = { outSampleFmt_, -1 };
    ret = av_opt_set_int_list(filterSinkContext_, "sample_fmts",
                              outSampleFmts_, -1, AV_OPT_SEARCH_CHILDREN);
    if (ret < 0) {
        EXO_LAVC_ERROR("av_opt_set_int_list(out, format)", ret);
        return false;
    }

    const int outSampleRates_[] = { static_cast<int>(pcmFormat_.rate), -1 };
    ret = av_opt_set_int_list(filterSinkContext_, "sample_rates",
                              outSampleRates_, -1, AV_OPT_SEARCH_CHILDREN);
    if (ret < 0) {
        EXO_LAVC_ERROR("av_opt_set_int_list(out, rate)", ret);
        return false;
    }

    av_channel_layout_describe(&outChLayout_,
                        channelDescription, sizeof(channelDescription));
    ret = av_opt_set(filterSinkContext_, "ch_layouts", channelDescription,
                              AV_OPT_SEARCH_CHILDREN);
    if (ret < 0) {
        EXO_LAVC_ERROR("av_opt_set_int_list(out, channels)", ret);
        return false;
    }

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

    {
        std::ostringstream filterDescriptionStream;
        filterDescriptionStream.imbue(std::locale::classic());

        if (params_.applyReplayGain) {
            filterDescriptionStream
                    << "volume="
                        "replaygain=track:"
                        "replaygain_preamp=" << params_.replayGainPreamp << ":"
                        "replaygain_noclip=" << params_.replayGainAntipeak;
            if (params_.r128Fix && ((hasR128Gain_ && !hasReplayGain_)
                                || exo::alwaysApplyR128Fix(codecContext_))) {
                filterDescriptionStream
                            << ":volume="
                            << exo::convertR128ToRG(0.0) << "dB";
            }
            filterDescriptionStream << ",";
        }

        filterDescriptionStream <<
                "aresample=" << pcmFormat_.rate << ","
                "aformat=sample_fmts=" <<
                    av_get_sample_fmt_name(outSampleFmt_) << ":"
                    // still has outChLayout_
                        "channel_layouts=" << channelDescription;

        auto filterDesc = filterDescriptionStream.str();
#if EXO_LAVC_DEBUG
        EXO_LOG("avfilter_graph_parse_ptr    %s", filterDesc.c_str());
#endif
        if ((ret = avfilter_graph_parse_ptr(filterGraph_, filterDesc.c_str(),
                                            &inputs, &outputs, nullptr)) < 0) {
            EXO_LAVC_ERROR("avfilter_graph_parse_ptr", ret);
            return false;
        }
    }

    if ((ret = avfilter_graph_config(filterGraph_, nullptr)) < 0) {
        EXO_LAVC_ERROR("avfilter_graph_config", ret);
        return false;
    }

    return true;
}

int LavcDecodeJob::filterFrames_(std::shared_ptr<exo::PcmSplitter>& sink,
                                 bool flush) {
    int ret;
    while (EXO_LIKELY(exo::shouldRun()) &&
            (ret = av_buffersink_get_frame(filterSinkContext_,
                                            filterFrame_)) >= 0) {
        std::size_t dataSize = filterFrame_->nb_samples
                    * filterFrame_->ch_layout.nb_channels
                    * av_get_bytes_per_sample(
                            static_cast<AVSampleFormat>(filterFrame_->format));
        sink->pcm({ filterFrame_->data[0], dataSize });
        av_frame_unref(filterFrame_);
    }
    if (ret < 0 && (ret != AVERROR(EAGAIN) && ret != AVERROR_EOF)) {
        EXO_LAVC_ERROR("av_buffersink_get_frame", ret);
        av_frame_unref(filterFrame_);
        av_frame_unref(frame_);
        return ret;
    }
    return 0;
}

#else // !USE_LIBAVFILTER

static constexpr std::size_t REPLAYGAIN_FRAC_BITS = 12;

using GainFixed = decltype(exo::LavcGain::i);
using GainFloat = decltype(exo::LavcGain::f);

template <exo::PcmSampleFormat fmt>
using GainType = std::conditional_t<exo::IsSampleFloatingPoint_v<fmt>,
                                    decltype(exo::LavcGain::f),
                                    decltype(exo::LavcGain::i)>;

template <std::signed_integral T>
T applyGainToSampleSigned_(T sample, exo::GainFixed gain) {
    using Wide = exo::WiderType_t<T>;
    auto x = (static_cast<Wide>(sample) * gain) >> REPLAYGAIN_FRAC_BITS;
    return static_cast<T>(std::clamp(x,
                          static_cast<Wide>(std::numeric_limits<T>::min()),
                          static_cast<Wide>(std::numeric_limits<T>::max())));
}

template <std::unsigned_integral T>
std::make_signed_t<T> convertUnsignedToSigned_(T sample) {
    using SignedT = std::make_signed_t<T>;
    using Wide = exo::WiderType_t<T>;
    return static_cast<SignedT>(static_cast<Wide>(sample)
                + static_cast<Wide>(std::numeric_limits<SignedT>::min()));
}

template <std::signed_integral T>
std::make_unsigned_t<T> convertSignedToUnsigned_(T sample) {
    using UnsignedT = std::make_unsigned_t<T>;
    using Wide = exo::WiderType_t<T>;
    return static_cast<UnsignedT>(static_cast<Wide>(sample)
            - static_cast<Wide>(std::numeric_limits<T>::min()));
}

template <std::unsigned_integral T>
T applyGainToSampleUnsigned_(T sample, exo::GainFixed gain) {
    return exo::convertSignedToUnsigned_(
            exo::applyGainToSampleSigned_(
                    exo::convertUnsignedToSigned_(sample), gain));
}

template <std::floating_point T>
T applyGainToSampleFloat_(T sample, exo::GainFloat gain) {
    return std::clamp(static_cast<T>(sample * gain), T{-1}, T{1});
}

template <exo::PcmSampleFormat fmt>
struct false_ : std::false_type { };

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
                dst, frames * channels,                                        \
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
    if (rejectMask_ & mask) return false;
    hasMask_ |= mask;
    rejectMask_ |= mask;
    return true;
}

void LavcGainCalculator::accept() noexcept {
    rejectMask_ = 0;
}

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

        /* if we are still accepting ReplayGain, then it's from a lower
           priority source, so pretend we do not have it */
        if (!(rejectMask_ & MASK_REPLAYGAIN))
            hasMask_ &= ~MASK_REPLAYGAIN;
    }
}

template <typename T>
requires (std::is_arithmetic_v<T>)
static T exp10(T value) {
    return std::pow(T{10}, value);
}

std::optional<double> LavcGainCalculator::gain(
                    bool antipeak, double preamp) const noexcept {
    double rg;
    if (has_(MASK_REPLAYGAIN)) {
        rg = replayGain_;
    } else if (has_(MASK_R128GAIN)) {
        rg = exo::convertR128ToRG(r128Gain_);
    } else {
#if EXO_LAVC_DEBUG
        EXO_LOG("no ReplayGain found, not applying");
#endif
        return { };
    }

#if EXO_LAVC_DEBUG
    EXO_LOG("detected ReplayGain %+.2f dB", rg);
#endif

    // compute final gain factor to apply
    double volume = exo::exp10((rg + preamp) * 0.05);

    // apply peak prevention
    if (antipeak && has_(MASK_REPLAYGAIN) && has_(MASK_REPLAYGAIN_PEAK))
        volume = std::min(volume, 1.0 / replayGainPeak_);

    return volume;
}

void LavcDecodeJob::calculateGain_() {
    if (!params_.applyReplayGain) return;

    auto gain = gainCalculator_.gain(
                params_.replayGainAntipeak, params_.replayGainPreamp);
    if (gain.has_value()) {
        auto volume = gain.value();
        if (exo::areSamplesFloatingPoint(pcmFormat_.sample)) {
            gain_.f = volume;
        } else {
            gain_.i = static_cast<decltype(gain_.i)>(0.5 +
                    volume * std::exp2(REPLAYGAIN_FRAC_BITS));
        }
    } else {
        params_.applyReplayGain = false;
    }
}

int LavcDecodeJob::processBuffer_(std::shared_ptr<exo::PcmSplitter>& sink,
                    exo::byte* buffer, std::size_t frameCount) {
    int size = av_samples_get_buffer_size(nullptr,
                        outChLayout_.nb_channels,
                        static_cast<int>(frameCount), outSampleFmt_, 1);
    if (EXO_UNLIKELY(size < 0)) {
        EXO_LAVC_ERROR("av_samples_get_buffer_size", size);
        return size;
    }

    if (params_.applyReplayGain)
        exo::applyReplayGain(pcmFormat_, buffer, frameCount, gain_);
    sink->pcm({ buffer, static_cast<std::size_t>(size) });
    return 0;
}

int LavcDecodeJob::processFrame_(std::shared_ptr<exo::PcmSplitter>& sink,
                                 const AVFrame* frame) {
    if (!frame) {
        // flush
        while (EXO_LIKELY(exo::shouldRun(exo::QuitStatus::NO_MORE_JOBS))) {
            exo::byte* out = buffer_;
            int gotFrames = swr_convert(resamplerContext_, &out,
                            bufferFrameCount_, nullptr, 0);
            if (EXO_UNLIKELY(gotFrames < 0)) {
                EXO_LAVC_ERROR("swr_convert", gotFrames);
                return gotFrames;
            }
            if (!gotFrames) break;

            int err = processBuffer_(sink, out, gotFrames);
            if (err < 0) return err;
        }

    } else {
        // accept frame
        int framesToExpect = swr_get_out_samples(
                        resamplerContext_, frame->nb_samples);
        int inCount = frame->nb_samples;
        const exo::byte* in = frame->data[0];

        while (EXO_LIKELY(exo::shouldRun(exo::QuitStatus::NO_MORE_JOBS))
                && framesToExpect > 0) {
            exo::byte* out = buffer_;
            int expectFrames = std::min(framesToExpect, bufferFrameCount_);
            int gotFrames = swr_convert(resamplerContext_, &out,
                            expectFrames, &in, inCount);
            if (EXO_UNLIKELY(gotFrames < 0)) {
                EXO_LAVC_ERROR("swr_convert", gotFrames);
                return gotFrames;
            }
            if (!gotFrames) break;
            framesToExpect -= gotFrames;
            inCount = 0; // do not refeed the same frame

            int err = processBuffer_(sink, out, gotFrames);
            if (err < 0) return err;
            if (gotFrames < expectFrames) break;
        }
    }
    return 0;
}
#endif // USE_LIBAVFILTER

void LavcDecodeJob::flush_(std::shared_ptr<exo::PcmSplitter>& sink) {
#if USE_LIBAVFILTER
    filterFrames_(sink, true);
#else
    processFrame_(sink, nullptr);
#endif
}

static exo::CaseInsensitiveMap<std::string> normalizedVorbisCommentKeys = {
    { "album_artist", "ALBUMARTIST" },
    { "track", "TRACKNUMBER" },
    { "disc", "DISCNUMBER" },
    { "comment", "DESCRIPTION" },
};

void LavcDecodeJob::readMetadata_(const AVDictionary* metadict) {
    const AVDictionaryEntry* tag = nullptr;
#if !USE_LIBAVFILTER
    gainCalculator_.accept();
#endif

    while ((tag = av_dict_iterate(metadict, tag))) {
        if (!exo::strnicmp(tag->key, "REPLAYGAIN_", 11)) {
            // do not forward ReplayGain tags
#if USE_LIBAVFILTER
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
#if USE_LIBAVFILTER
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
            auto it = exo::normalizedVorbisCommentKeys.find(
                                    std::string(tag->key));
            if (it != exo::normalizedVorbisCommentKeys.end())
                metadata_.push_back({ it->second, tag->value });
            else
                metadata_.push_back({ tag->key, tag->value });

        } else {
            metadata_.push_back({ tag->key, tag->value });
        }
    }
}

void LavcDecodeJob::run(std::shared_ptr<exo::PcmSplitter> sink) {
    if (!canPlay_) return;

    sink->metadata(command_, metadata_);
    int ret;
    while ((ret = av_read_frame(formatContext_, packet_) >= 0) &&
                    EXO_LIKELY(exo::shouldRun())) {
        if (packet_->stream_index == streamIndex_) {
            if (EXO_UNLIKELY((ret = avcodec_send_packet(
                        codecContext_, packet_)) < 0)) {
                EXO_LAVC_ERROR("avcodec_send_packet", ret);
                break;
            }
        }
        av_packet_unref(packet_);

        ret = decodeFrames_(sink, false);
        if (EXO_UNLIKELY(ret != AVERROR(EAGAIN)))
            break;
    }
    if (EXO_UNLIKELY(ret < 0 && ret != AVERROR_EOF))
        EXO_LAVC_ERROR("av_read_frame", ret);

    // flush codec
    if (EXO_UNLIKELY((ret = avcodec_send_packet(codecContext_, nullptr)) < 0))
        EXO_LAVC_ERROR("avcodec_send_packet", ret);
    decodeFrames_(sink, true);

    // flush resampler
#if USE_LIBAVFILTER
    if (av_buffersrc_add_frame_flags(bufferSourceContext_, nullptr, 0) < 0)
        EXO_LAVC_ERROR("av_buffersrc_add_frame_flags(EOF)", ret);
#else
    if (EXO_UNLIKELY((ret = swr_convert(resamplerContext_,
                    nullptr, 0, nullptr, 0)) < 0))
        EXO_LAVC_ERROR("swr_convert(EOF)", ret);
#endif
    flush_(sink);
}

LavcDecodeJob::~LavcDecodeJob() noexcept {
#if USE_LIBAVFILTER
    if (EXO_LIKELY(filterGraph_)) avfilter_graph_free(&filterGraph_);
    // freeing the graph will also free the contexts
    else {
        if (filterSinkContext_) avfilter_free(filterSinkContext_);
        if (bufferSourceContext_) avfilter_free(bufferSourceContext_);
    }
    if (EXO_LIKELY(filterFrame_)) av_frame_free(&filterFrame_);
#else
    if (EXO_LIKELY(resamplerContext_)) {
        swr_close(resamplerContext_);
        swr_free(&resamplerContext_);
    }
    if (EXO_LIKELY(buffer_)) av_freep(&buffer_);
#endif
    if (EXO_LIKELY(codecContext_)) avcodec_free_context(&codecContext_);
    if (EXO_LIKELY(formatContext_)) avformat_close_input(&formatContext_);
    if (EXO_LIKELY(frame_)) av_frame_free(&frame_);
    if (EXO_LIKELY(packet_)) av_packet_free(&packet_);
}

} // namespace exo
