/***
exocaster -- audio streaming helper
encoder/libvorbis/oggvorbis.cc -- Ogg Vorbis encoder using libvorbis

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
#include <exception>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include "encoder/libvorbis/oggvorbis.hh"
#include "log.hh"
#include "pcmconvert.hh"
#include "pcmtypes.hh"
#include "resampler/resampler.hh"
#include "server.hh"
#include "streamformat.hh"
#include "util.hh"

extern "C" {
#include <ogg/ogg.h>
#include <vorbis/codec.h>
#include <vorbis/vorbisenc.h>
}

namespace exo {

exo::VorbisInfo::VorbisInfo() { vorbis_info_init(get()); }

exo::VorbisInfo::~VorbisInfo() noexcept { vorbis_info_clear(get()); }

exo::VorbisDspState::VorbisDspState(exo::VorbisInfo& info) {
    int err = vorbis_analysis_init(get(), info.get());
    if (err) {
        EXO_LOG("vorbis_analysis_init failed: %d", err);
        throw std::runtime_error("vorbis_analysis_init failed");
    }
}

exo::VorbisDspState::~VorbisDspState() noexcept { vorbis_dsp_clear(get()); }

exo::VorbisBlock::VorbisBlock(exo::VorbisDspState& dspState) {
    int err = vorbis_block_init(dspState.get(), get());
    if (err) {
        EXO_LOG("vorbis_block_init failed: %d", err);
        throw std::runtime_error("vorbis_block_init failed");
    }
}

exo::VorbisBlock::~VorbisBlock() noexcept { vorbis_block_clear(get()); }

exo::VorbisComment::VorbisComment() { vorbis_comment_init(get()); }

exo::VorbisComment::~VorbisComment() noexcept { vorbis_comment_clear(get()); }

exo::OggVorbisEncoder::OggVorbisEncoder(
    const exo::ConfigObject& config, std::shared_ptr<exo::PcmBuffer> source,
    exo::PcmFormat pcmFormat, const exo::ResamplerFactory& resamplerFactory)
    : BaseEncoder(source, pcmFormat) {
    nomBitrate_ = cfg::namedInt<std::int_least32_t>(config, "bitrate", 128000);
    minBitrate_ = cfg::namedInt<std::int_least32_t>(config, "minbitrate", -1);
    maxBitrate_ = cfg::namedInt<std::int_least32_t>(config, "maxbitrate", -1);

    switch (pcmFormat.channels) {
    case exo::PcmChannelLayout::Mono:
        channels_ = 1;
        break;
    case exo::PcmChannelLayout::Stereo:
        channels_ = 2;
        break;
    default:
        throw std::runtime_error("unsupported channel layout for oggvorbis");
    }

    std::random_device dev;
    std::uniform_int_distribution<std::uint32_t> dist(0, UINT32_MAX);
    serial_ = static_cast<std::uint32_t>(dist(dev));
}

exo::StreamFormat exo::OggVorbisEncoder::streamFormat() const noexcept {
    return exo::EncodedStreamFormat{exo::EncodedStreamFormatCodec::OGG_VORBIS};
}

void exo::OggVorbisEncoder::pushPage_(const ogg_page& page) {
    std::uint_least64_t newGranulePosition = ogg_page_granulepos(&page);
    std::size_t granules = newGranulePosition - lastGranulePosition_;

    packet(0, {page.header, static_cast<std::size_t>(page.header_len)});
    packet(granules, {page.body, static_cast<std::size_t>(page.body_len)});

    granulesInPage_ -= granules;
    lastGranulePosition_ = newGranulePosition;
    endOfStream_ = ogg_page_eos(&page);
}

void exo::OggVorbisEncoder::startTrack(const exo::Metadata& metadata) {
    if (init_)
        endTrack();

    // ensure correct destruction order
    stream_.reset();
    block_.reset();
    dspState_.reset();
    comment_.reset();
    info_.reset();

    auto info = (info_ = std::make_unique<VorbisInfo>())->get();
    int ret =
        vorbis_encode_setup_managed(info, channels_, pcmFormat_.rate,
                                    maxBitrate_, nomBitrate_, minBitrate_);
    if (ret) {
        EXO_LOG("oggvorbis: vorbis_encode_setup_managed failed (%d). "
                "skipping track.",
                ret);
        return;
    }

    ret = vorbis_encode_ctl(info, OV_ECTL_RATEMANAGE2_SET, nullptr);
    if (ret) {
        EXO_LOG("oggvorbis: vorbis_encode_ctl failed (%d). "
                "skipping track.",
                ret);
        return;
    }

    ret = vorbis_encode_setup_init(info);
    if (ret) {
        EXO_LOG("oggvorbis: vorbis_encode_setup_init failed (%d). "
                "skipping track.",
                ret);
        return;
    }

    vorbis_comment* comment;
    vorbis_dsp_state* dsp;
    ogg_stream_state* stream;

    try {
        comment = (comment_ = std::make_unique<VorbisComment>())->get();
        dsp = (dspState_ = std::make_unique<VorbisDspState>(*info_))->get();
        block_ = std::make_unique<VorbisBlock>(*dspState_);
        stream = (stream_ = std::make_unique<OggStreamState>(
                      static_cast<int>(serial_++)))
                     ->get();
    } catch (std::exception& e) {
        EXO_LOG("vorbis track change failed. skipping track.");
        return;
    }

    for (const auto& [key, value] : metadata)
        vorbis_comment_add_tag(comment, key.c_str(), value.c_str());

    std::array<ogg_packet, 3> heads;
    vorbis_analysis_headerout(dsp, comment, &heads[0], &heads[1], &heads[2]);
    for (unsigned i = 0; i < heads.size(); ++i) {
        if (ogg_stream_packetin(stream, &heads[i]) < 0) {
            EXO_LOG("ogg_stream_packetin failed");
            return;
        }
    }

    granulesInPage_ = 0;
    lastGranulePosition_ = 0;
    flushPages_();
    endOfStream_ = false;
    init_ = true;
}

void exo::OggVorbisEncoder::flushBuffers_() {
    auto dsp = dspState_->get();
    auto block = block_->get();
    auto stream = stream_->get();

    ogg_page page;
    ogg_packet packet;
    const unsigned long flushThreshold = pcmFormat_.rate * 2;

    while (vorbis_analysis_blockout(dsp, block) == 1 &&
           EXO_LIKELY(exo::shouldRun())) {
        vorbis_analysis(block, nullptr);
        vorbis_bitrate_addblock(block);

        while (vorbis_bitrate_flushpacket(dsp, &packet) &&
               EXO_LIKELY(exo::shouldRun())) {
            if (ogg_stream_packetin(stream, &packet) < 0) {
                EXO_LOG("ogg_stream_packetin failed");
                endTrack();
                return;
            }

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
    }
}

void exo::OggVorbisEncoder::flushPages_() {
    if (stream_) {
        ogg_page page;
        auto stream = stream_->get();
        while (ogg_stream_flush(stream, &page))
            pushPage_(page);
    }
}

template <exo::PcmSampleFormat fmt>
static void uninterleaveToFloat_(float** dst, const void* srcv,
                                 unsigned channels, std::size_t frames) {
    auto src = reinterpret_cast<const exo::PcmFormat_t<fmt>*>(srcv);

    for (std::size_t c = 0; c < channels; ++c) {
        auto sample = src;

        for (std::size_t s = 0; s < frames; ++s) {
            dst[c][s] = exo::convertSampleToFloat<fmt, float>(sample[c]);
            sample += channels;
        }
    }
}

static void uninterleaveToFloat(exo::PcmFormat pcmfmt, float** dst,
                                const void* src, std::size_t frames) {
    auto channels = exo::channelCount(pcmfmt.channels);
    switch (pcmfmt.sample) {
#define EXO_PCM_FORMATS_CASE(F)                                                \
    case exo::PcmSampleFormat::F:                                              \
        return exo::uninterleaveToFloat_<exo::PcmSampleFormat::F>(             \
            dst, src, channels, frames);
        EXO_PCM_FORMATS_SWITCH
#undef EXO_PCM_FORMATS_CASE
    default:
        EXO_UNREACHABLE;
    }
}

void exo::OggVorbisEncoder::pcmBlock(std::size_t frameCount,
                                     std::span<const exo::byte> data) {
    if (!init_)
        return;

    const byte* source = data.data();
    std::size_t count = data.size();
    if (!count)
        return;

    alignas(std::uintmax_t) exo::byte alignedBuffer[4096] = {0};
    auto dsp = dspState_->get();
    auto fitFrames = sizeof(alignedBuffer) / pcmFormat_.bytesPerFrame();

    float** dspBuffer = vorbis_analysis_buffer(dsp, fitFrames);
    const auto pcmFormat = pcmFormat_;
    while (count > 0 && EXO_LIKELY(exo::shouldRun())) {
        std::size_t size = std::min(count, sizeof(alignedBuffer));
        auto frames = size / pcmFormat_.bytesPerFrame();

        // copy sample data to aligned buffer
        std::copy(source, source + size, alignedBuffer);
        source += size, count -= size;

        // convert and submit samples
        exo::uninterleaveToFloat(pcmFormat, dspBuffer, alignedBuffer, frames);
        vorbis_analysis_wrote(dsp, frames);
        granulesInPage_ += frames;
        flushBuffers_();
    }
}

void exo::OggVorbisEncoder::endTrack() {
    if (!init_)
        return;
    vorbis_analysis_wrote(dspState_->get(), 0);
    flushBuffers_();
    flushPages_();
    init_ = false;
}

} // namespace exo
