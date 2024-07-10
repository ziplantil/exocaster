/***
exocaster -- audio streaming helper
encoder/libopus/oggopus.cc -- Ogg Opus encoder using libopus

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
#include <array>
#include <bit>
#include <cstring>
#include <exception>
#include <initializer_list>
#include <memory>
#include <new>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include "debug.hh"
#include "encoder/libopus/oggopus.hh"
#include "log.hh"
#include "pcmconvert.hh"
#include "pcmtypes.hh"
#include "resampler/resampler.hh"
#include "resampler/types.hh"
#include "server.hh"
#include "streamformat.hh"
#include "util.hh"

extern "C" {
#include <ogg/ogg.h>
#include <opus/opus.h>
#include <opus/opus_defines.h>
#include <opus/opus_types.h>
}

namespace exo {

exo::OpusEncoder::OpusEncoder(opus_int32 Fs, int channels, int application)
    : PointerSlot(nullptr) {
    int error;
    set() = opus_encoder_create(Fs, channels, application, &error);
    if (!has()) {
        EXO_LOG("opus_encoder_create failed (%d): %s", error,
                opus_strerror(error));
        switch (error) {
        case OPUS_ALLOC_FAIL:
            throw std::bad_alloc();
        default:
            throw std::runtime_error("opus_encoder_create failed");
        }
    }
}

exo::OpusEncoder::~OpusEncoder() noexcept {
    if (has())
        opus_encoder_destroy(release());
}

exo::OggOpusEncoder::OggOpusEncoder(
    const exo::ConfigObject& config, std::shared_ptr<exo::PcmBuffer> source,
    exo::PcmFormat pcmFormat, const exo::ResamplerFactory& resamplerFactory)
    : BaseEncoder(source, pcmFormat), encoder_(nullptr) {
    bitrate_ = cfg::namedInt<std::int_least32_t>(config, "bitrate", 0);
    complexity_ = cfg::namedInt<std::int_least32_t>(config, "complexity", 10);

    if (complexity_ < 0 || complexity_ > 10)
        throw std::runtime_error("oggopus complexity out of range [0, 10]");

    switch (pcmFormat.channels) {
    case exo::PcmChannelLayout::Mono:
        channels_ = 1;
        break;
    case exo::PcmChannelLayout::Stereo:
        channels_ = 2;
        break;
    default:
        throw std::runtime_error("unsupported channel layout for oggopus");
    }

    if (channels_ < 1 || channels_ > 8)
        throw std::runtime_error("unsupported channel layout for oggopus");

    std::random_device dev;
    std::uniform_int_distribution<std::uint32_t> dist(0, UINT32_MAX);
    serial_ = static_cast<std::uint32_t>(dist(dev));

    rate_ = 48000U;
    resampler_ = resamplerFactory.createResampler(rate_);

    switch (rate_) {
    case 8000U:
    case 12000U:
    case 16000U:
    case 24000U:
    case 48000U:
        break;
    default:
        throw std::runtime_error("unsupported sample rate for oggopus");
    }

    // prefer full 20 ms packets
    if (rate_ % 50)
        throw std::runtime_error("unsupported sample rate for oggopus");
    pcm_.resize(channels_ * rate_ / 50, 0.0f);
    mid_.resize(pcm_.size(), 0.0f);
    opus_.resize(MAX_OPUS_PACKET_SIZE, 0);
}

exo::StreamFormat exo::OggOpusEncoder::streamFormat() const noexcept {
    return exo::EncodedStreamFormat{exo::EncodedStreamFormatCodec::OGG_OPUS};
}

std::size_t exo::OggOpusEncoder::outputFrameRate() const noexcept {
    return rate_;
}

static void opusError_(const char* file, std::size_t lineno, const char* fn,
                       int ret) {
    exo::log(file, lineno, "%s failed (%d): %s", fn, ret, opus_strerror(ret));
}

#define EXO_OPUS_ERROR(fn, ret) exo::opusError_(__FILE__, __LINE__, fn, ret)

void exo::OggOpusEncoder::pushPage_(const ogg_page& page) {
    std::uint_least64_t newGranulePosition = ogg_page_granulepos(&page);
    std::size_t granules = newGranulePosition - lastGranulePosition_;

    packet(0, {page.header, static_cast<std::size_t>(page.header_len)});
    packet(granules, {page.body, static_cast<std::size_t>(page.body_len)});

    granulesInPage_ -= granules;
    lastGranulePosition_ = newGranulePosition;
    endOfStream_ = ogg_page_eos(&page);
}

struct OggOpusHeader {
    std::uint8_t version{1};
    std::uint8_t channels;
    std::uint16_t preskip;
    std::uint32_t sampleRate;
    std::int16_t gain{0};
    std::uint8_t channelMap{0};
    std::uint8_t streamCount{1};
    std::uint8_t twoChannelStreamCount{1};
    std::array<std::uint8_t, 256> channelMapTable{0};
};

using OpusHeaderPacket = std::array<exo::byte, sizeof(OggOpusHeader)>;

static std::size_t makeOggOpusHeader(OpusHeaderPacket& arr,
                                     const OggOpusHeader& header) {
    exo::byte* dst = arr.data();
    std::memcpy(dst, "OpusHead", 8), dst += 8;
    *dst++ = header.version;
    *dst++ = header.channels;
    *dst++ = static_cast<std::uint8_t>(header.preskip & 0xFFU);
    *dst++ = static_cast<std::uint8_t>((header.preskip >> 8) & 0xFFU);
    *dst++ = static_cast<std::uint8_t>(header.sampleRate & 0xFFU);
    *dst++ = static_cast<std::uint8_t>((header.sampleRate >> 8) & 0xFFU);
    *dst++ = static_cast<std::uint8_t>((header.sampleRate >> 16) & 0xFFU);
    *dst++ = static_cast<std::uint8_t>((header.sampleRate >> 24) & 0xFFU);
    *dst++ = static_cast<std::uint8_t>(
        std::bit_cast<std::uint16_t>(header.gain) & 0xFFU);
    *dst++ = static_cast<std::uint8_t>(
        (std::bit_cast<std::uint16_t>(header.gain) >> 8) & 0xFFU);
    *dst++ = header.channelMap;
    if (header.channelMap > 0) {
        *dst++ = header.streamCount;
        *dst++ = header.twoChannelStreamCount;
        std::memcpy(dst, header.channelMapTable.data(), header.channels);
        dst += header.channels;
    }
    return static_cast<std::size_t>(dst - arr.data());
}

template <typename T, std::size_t... I>
static std::initializer_list<exo::byte>
asBytes_impl_(const T& a, std::index_sequence<I...>) {
    return {static_cast<exo::byte>(a[I])...};
}

template <std::size_t N, typename Indices = std::make_index_sequence<N - 1>>
    requires(N > 0)
static std::initializer_list<exo::byte> asBytes_(const char (&s)[N]) {
    return exo::asBytes_impl_(s, Indices{});
}

static bool addSize(std::vector<exo::byte>& v, std::size_t z) {
    if (z > UINT32_MAX)
        return false;
    v.reserve(v.size() + 4);
    v.push_back(static_cast<exo::byte>(z & 0xFFU));
    v.push_back(static_cast<exo::byte>((z >> 8) & 0xFFU));
    v.push_back(static_cast<exo::byte>((z >> 16) & 0xFFU));
    v.push_back(static_cast<exo::byte>((z >> 24) & 0xFFU));
    return true;
}

static bool addString(std::vector<exo::byte>& v, const std::string_view& s) {
    auto p = reinterpret_cast<const exo::byte*>(s.data());
    if (exo::addSize(v, s.size())) {
        v.insert(v.end(), p, p + s.size());
        return true;
    } else
        return false;
}

static bool canAddField(std::vector<exo::byte>& v, const std::string_view& key,
                        const std::string_view& value) {
    std::size_t total = key.size() + value.size() + 1;
    return total < INT32_MAX;
}

static bool addField(std::vector<exo::byte>& v, const std::string_view& key,
                     const std::string_view& value) {
    std::size_t total = key.size() + value.size() + 1;
    if (!exo::addSize(v, total))
        return false;
    auto p = reinterpret_cast<const exo::byte*>(key.data());
    v.insert(v.end(), p, p + key.size());
    v.push_back('=');
    p = reinterpret_cast<const exo::byte*>(value.data());
    v.insert(v.end(), p, p + value.size());
    return true;
}

static void makeOggOpusMeta(std::vector<exo::byte>& v,
                            const exo::Metadata& metadata) {
    const char tag[] = "OpusTags";
    auto tag_ = reinterpret_cast<const exo::byte*>(tag);
    v.insert(v.end(), tag_, tag_ + sizeof(tag) - 1);

    if (!exo::addString(v, opus_get_version_string()))
        throw std::runtime_error("vendor string is absurdly long");

    std::size_t fields = 0;
    for (std::size_t i = 0; i < metadata.size(); ++i)
        if (exo::canAddField(v, metadata[i].first, metadata[i].second))
            ++fields;

    fields = std::min(fields, static_cast<std::size_t>(UINT32_MAX));
    if (!exo::addSize(v, fields))
        throw std::runtime_error("too many metadata fields");

    for (std::size_t i = 0; i < fields; ++i)
        exo::addField(v, metadata[i].first, metadata[i].second);
}

ogg_packet exo::OggOpusEncoder::makeOggPacket_(std::span<byte> data, bool eos) {
    bool bos = packetIndex_ == 0;
    return ogg_packet{.packet = data.data(),
                      .bytes = static_cast<long>(data.size()),
                      .b_o_s = bos ? 1 : 0,
                      .e_o_s = eos ? 1 : 0,
                      .granulepos = static_cast<ogg_int64_t>(granuleIndex_),
                      .packetno = static_cast<ogg_int64_t>(packetIndex_++)};
}

void exo::OggOpusEncoder::startTrack(const exo::Metadata& metadata) {
    if (init_)
        endTrack();

    // ensure correct destruction order
    stream_.reset();

    encoder_ = OpusEncoder(rate_, channels_, OPUS_APPLICATION_AUDIO);
    auto enc = encoder_.get();

    int ret;
    if ((ret = opus_encoder_ctl(enc, OPUS_SET_DTX(0))) < 0) {
        EXO_OPUS_ERROR("opus_encoder_ctl(OPUS_SET_DTX)", ret);
        return;
    }

    if (bitrate_ > 0) {
        if ((ret = opus_encoder_ctl(enc, OPUS_SET_BITRATE(bitrate_))) < 0) {
            EXO_OPUS_ERROR("opus_encoder_ctl(OPUS_SET_BITRATE)", ret);
            return;
        }
    } else if (bitrate_ < 0) {
        if ((ret = opus_encoder_ctl(enc, OPUS_SET_BITRATE(OPUS_BITRATE_MAX))) <
            0) {
            EXO_OPUS_ERROR("opus_encoder_ctl(OPUS_SET_BITRATE)", ret);
            return;
        }
    }

    int lookahead;
    if ((ret = opus_encoder_ctl(enc, OPUS_GET_LOOKAHEAD(&lookahead))) < 0) {
        EXO_OPUS_ERROR("opus_encoder_ctl(OPUS_GET_LOOKAHEAD)", ret);
        return;
    }

    if ((ret = opus_encoder_ctl(enc, OPUS_SET_COMPLEXITY(complexity_))) < 0) {
        EXO_OPUS_ERROR("opus_encoder_ctl(OPUS_SET_COMPLEXITY)", ret);
        return;
    }

    ogg_stream_state* stream;
    ogg_packet packet;

    try {
        stream = (stream_ = std::make_unique<OggStreamState>(
                      static_cast<int>(serial_++)))
                     ->get();
    } catch (std::exception& e) {
        return;
    }

    packetIndex_ = 0;
    granuleIndex_ = 0;

    // Opus header packet
    OpusHeaderPacket opusHeader;
    std::size_t opusHeaderSize = exo::makeOggOpusHeader(
        opusHeader,
        exo::OggOpusHeader{.version = 1,
                           .channels = static_cast<std::uint8_t>(channels_),
                           .preskip = static_cast<std::uint16_t>(lookahead),
                           .sampleRate =
                               static_cast<std::uint32_t>(pcmFormat_.rate),
                           .gain = 0});
    packet = makeOggPacket_({opusHeader.data(), opusHeaderSize});
    if (ogg_stream_packetin(stream, &packet) < 0) {
        EXO_LOG("ogg_stream_packetin failed");
        return;
    }

    // Vorbis comment packet
    {
        std::vector<exo::byte> metaPacket;
        exo::makeOggOpusMeta(metaPacket, metadata);
        packet = makeOggPacket_({metaPacket.begin(), metaPacket.end()});
        if (ogg_stream_packetin(stream, &packet) < 0) {
            EXO_LOG("ogg_stream_packetin failed");
            return;
        }
    }

    granulesInPage_ = 0;
    lastGranulePosition_ = 0;
    lastToc_ = channels_ > 1 ? 0x3C : 0x1C;
    flushPages_();
    endOfStream_ = false;
    init_ = true;

    // feed silence to compensate for lookahead
    std::size_t precomp = static_cast<std::size_t>(lookahead) * channels_;
    std::fill(pcm_.begin(), pcm_.end(), 0.0f);
    const auto size = pcm_.size();
    while (precomp >= size) {
        precomp -= size;
        pcmIndex_ = size;
        flushBuffer_(false);
        EXO_ASSERT(pcmIndex_ == 0);
    }
    pcmIndex_ = precomp;
}

template <typename T, exo::PcmSampleFormat fmt>
static const T* convertSamplesToFloat_(float* dst, const T* srcv,
                                       std::size_t samples) {
    auto src = reinterpret_cast<const exo::PcmFormat_t<fmt>*>(srcv);
    for (std::size_t s = 0; s < samples; ++s)
        *dst++ = exo::convertSampleToFloat<fmt, float>(*src++);
    return reinterpret_cast<const T*>(src);
}

template <typename T>
static const T* convertSamplesToFloat(exo::PcmFormat pcmfmt, float* dst,
                                      const T* src, std::size_t samples) {
    switch (pcmfmt.sample) {
#define EXO_PCM_FORMATS_CASE(F)                                                \
    case exo::PcmSampleFormat::F:                                              \
        return exo::convertSamplesToFloat_<T, exo::PcmSampleFormat::F>(        \
            dst, src, samples);
        EXO_PCM_FORMATS_SWITCH
#undef EXO_PCM_FORMATS_CASE
    default:
        EXO_UNREACHABLE;
    }
}

void exo::OggOpusEncoder::flushBuffer_(bool force) {
    auto enc = encoder_.get();
    auto stream = stream_->get();
    std::size_t trueFrames = pcmIndex_ / channels_;

    if (pcmIndex_ < pcm_.size()) {
        if (!force)
            return; // not enough data yet

        // fill rest of block with silence
        std::fill(pcm_.begin() + pcmIndex_, pcm_.end(), 0.0f);
        pcmIndex_ = pcm_.size();
    }

    std::size_t sampleCount = std::exchange(pcmIndex_, 0);
    auto frameCount = sampleCount / channels_;
    EXO_ASSERT(trueFrames <= frameCount);

    int ret = opus_encode_float(enc, pcm_.data(), frameCount, opus_.data(),
                                opus_.size());
    if (ret < 0) {
        EXO_OPUS_ERROR("opus_encode_float", ret);
        init_ = false;
        return;
    }
    // no data returned?
    if (!ret)
        return;

    lastToc_ = opus_[0];

    // build ogg packet
    granuleIndex_ += trueFrames;
    ogg_packet packet =
        makeOggPacket_({opus_.data(), static_cast<std::size_t>(ret)});
    if (ogg_stream_packetin(stream, &packet) < 0) {
        EXO_LOG("ogg_stream_packetin failed");
        endTrack();
        return;
    }
    granulesInPage_ += trueFrames;

    const unsigned long flushThreshold = rate_ * 2;
    ogg_page page;
    while (EXO_LIKELY(!endOfStream_ && exo::shouldRun())) {
        int result;

        if (granulesInPage_ >= flushThreshold)
            result = ogg_stream_flush(stream, &page);
        else
            result = ogg_stream_pageout(stream, &page);

        if (!result)
            break;
        pushPage_(page);
    }
}

void exo::OggOpusEncoder::flushPages_() {
    if (stream_) {
        ogg_page page;
        auto stream = stream_->get();
        while (ogg_stream_flush(stream, &page))
            pushPage_(page);
    }
}

void exo::OggOpusEncoder::flushResampler_(std::span<const float> samples) {
    auto begin = samples.begin();

    while (EXO_LIKELY(exo::shouldRun()) && begin != samples.end()) {
        EXO_ASSERT(pcmIndex_ < pcm_.size());
        auto result = resampler_->resampleInterleaved(
            {pcm_.begin() + pcmIndex_, pcm_.end()}, {begin, samples.end()});
        begin += result.read * channels_;
        pcmIndex_ += result.wrote * channels_;
        if (pcmIndex_ == pcm_.size()) {
            flushBuffer_(false);
        }
    }
    EXO_ASSERT(pcmIndex_ < pcm_.size());
}

void exo::OggOpusEncoder::flushResampler_() {
    while (EXO_LIKELY(exo::shouldRun())) {
        auto wrote = resampler_->flushInterleaved(
            {pcm_.begin() + pcmIndex_, pcm_.end()});
        if (!wrote)
            break;
        pcmIndex_ += wrote * channels_;
        if (pcmIndex_ == pcm_.size()) {
            flushBuffer_(false);
        }
    }
}

void exo::OggOpusEncoder::pcmBlock(std::size_t frameCount,
                                   std::span<const exo::byte> data) {
    if (!init_)
        return;

    const byte* source = data.data();
    std::size_t count = data.size();
    if (!count)
        return;
    auto samples = count / pcmFormat_.bytesPerSample();

    std::size_t midFrames = mid_.size() / channels_;
    std::size_t midSamples = midFrames * channels_;

    while (EXO_LIKELY(exo::shouldRun()) && samples > 0) {
        std::size_t convertSamples = std::min(samples, midSamples);
        source = exo::convertSamplesToFloat(pcmFormat_, mid_.data(), source,
                                            convertSamples);
        samples -= convertSamples;
        flushResampler_({mid_.data(), convertSamples});
    }
}

std::size_t exo::OggOpusEncoder::makeFinalOpusFrame_() {
    std::size_t n = 0;
    // use the config from the last TOC, but with code 0
    // our packet will be a single byte and thus be empty,
    // but at least have a TOC, like every Opus frame must.
    opus_[n++] = lastToc_ & 0x3FU;
    return n;
}

void exo::OggOpusEncoder::endTrack() {
    if (!init_)
        return;
    flushResampler_();
    flushBuffer_(true);
    auto n = makeFinalOpusFrame_();
    ogg_packet packet = makeOggPacket_({opus_.data(), n}, true);
    ogg_stream_packetin(stream_->get(), &packet);
    flushPages_();
    init_ = false;
}

} // namespace exo
