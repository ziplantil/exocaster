/***
exocaster -- audio streaming helper
encoder/lame/mp3.cc -- MP3 encoder using LAME

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
#include <new>
#include <stdexcept>

#include "config.hh"
#include "encoder/lame/mp3.hh"
#include "log.hh"
#include "packet.hh"
#include "pcmconvert.hh"
#include "pcmtypes.hh"

extern "C" {
#include <lame/lame.h>
}

namespace exo {

static void lameError_(const char* file, std::size_t lineno, const char* fn,
                       int ret) {
    exo::log(file, lineno, "%s failed (%d)", fn, ret);
}

#define EXO_LAME_ERROR(fn, ret) exo::lameError_(__FILE__, __LINE__, fn, ret)

Lame::Lame() : PointerSlot(lame_init()) {
    if (!get())
        throw std::bad_alloc();
}

Lame::~Lame() noexcept {
    if (get())
        lame_close(release());
}

Mp3Encoder::Mp3Encoder(const exo::ConfigObject& config,
                       std::shared_ptr<exo::PcmBuffer> source,
                       exo::PcmFormat pcmFormat,
                       const exo::ResamplerFactory& resamplerFactory,
                       const std::shared_ptr<exo::Barrier>& barrier)
    : BaseEncoder(source, pcmFormat, barrier), lame_() {
    nomBitrate_ = cfg::namedInt<std::int_least32_t>(config, "bitrate", 320);
    minBitrate_ = cfg::namedInt<std::int_least32_t>(config, "minbitrate", -1);
    maxBitrate_ = cfg::namedInt<std::int_least32_t>(config, "maxbitrate", -1);
    vbr_ = cfg::namedBoolean(config, "vbr", false);

    switch (pcmFormat.channels) {
    case exo::PcmChannelLayout::Mono:
        channels_ = 1;
        break;
    case exo::PcmChannelLayout::Stereo:
        channels_ = 2;
        break;
    default:
        EXO_UNREACHABLE;
        throw std::runtime_error("mp3 encoder: unsupported channel layout");
    }
    buffer_.reserve(7200);
}

exo::StreamFormat Mp3Encoder::streamFormat() const noexcept {
    return exo::EncodedStreamFormat{exo::EncodedStreamFormatCodec::MP3};
}

void Mp3Encoder::startTrack(const exo::Metadata& metadata) {
    if (init_) {
        endTrack();
    }

    int err;
    lame_ = Lame();
    auto lame = lame_.get();

    // parameters
    if ((err = lame_set_in_samplerate(lame, pcmFormat_.rate)) < 0) {
        EXO_LAME_ERROR("lame_set_in_samplerate", err);
        return;
    }
    if ((err = lame_set_num_channels(lame, channels_)) < 0) {
        EXO_LAME_ERROR("lame_set_num_channels", err);
        return;
    }

    if (vbr_) {
        if (minBitrate_ >= 0 || maxBitrate_ >= 0) {
            if ((err = lame_set_VBR(lame, vbr_abr)) < 0) {
                EXO_LAME_ERROR("lame_set_VBR", err);
                return;
            }
            if ((err = lame_set_VBR_mean_bitrate_kbps(lame, nomBitrate_)) < 0) {
                EXO_LAME_ERROR("lame_set_VBR_mean_bitrate_kbps", err);
                return;
            }
            if (minBitrate_ >= 0 &&
                (err = lame_set_VBR_min_bitrate_kbps(lame, minBitrate_)) < 0) {
                EXO_LAME_ERROR("lame_set_VBR_min_bitrate_kbps", err);
                return;
            }
            if (maxBitrate_ >= 0 &&
                (err = lame_set_VBR_max_bitrate_kbps(lame, maxBitrate_)) < 0) {
                EXO_LAME_ERROR("lame_set_VBR_max_bitrate_kbps", err);
                return;
            }
        } else {
            if ((err = lame_set_VBR(lame, vbr_mtrh)) < 0) {
                EXO_LAME_ERROR("lame_set_VBR", err);
                return;
            }
        }
    } else if ((err = lame_set_brate(lame, nomBitrate_)) < 0) {
        EXO_LAME_ERROR("lame_set_brate", err);
        return;
    }

    if ((err = lame_set_quality(lame, 2)) < 0) {
        EXO_LAME_ERROR("lame_set_quality", err);
        return;
    }

    if ((err = lame_set_bWriteVbrTag(lame, 0)) < 0) {
        EXO_LAME_ERROR("lame_set_quality", err);
        return;
    }

    if ((err = lame_init_params(lame)) < 0) {
        EXO_LAME_ERROR("lame_init_params", err);
        return;
    }

    granule_ = 0;
    init_ = true;

    auto metaStr = exo::writeOutOfBandMetadata(metadata);
    packet(
        PacketFlags::OutOfBandMetadata, 0,
        {reinterpret_cast<const exo::byte*>(metaStr.data()), metaStr.size()});
}

template <exo::PcmSampleFormat fmt>
static void uninterleaveToFloat_(float* l, float* r, const void* srcv,
                                 bool mono, std::size_t frames) {
    auto src = reinterpret_cast<const exo::PcmFormat_t<fmt>*>(srcv);

    if (mono) {
        for (std::size_t s = 0; s < frames; ++s) {
            auto f = exo::convertSampleToFloat<fmt, float>(*src++);
            *l++ = *r++ = f;
        }
    } else {
        for (std::size_t s = 0; s < frames; ++s) {
            *l++ = exo::convertSampleToFloat<fmt, float>(*src++);
            *r++ = exo::convertSampleToFloat<fmt, float>(*src++);
        }
    }
}

static void uninterleaveToFloat(exo::PcmFormat pcmfmt, float* l, float* r,
                                const void* src, std::size_t frames) {
    auto channels = exo::channelCount(pcmfmt.channels);
    switch (pcmfmt.sample) {
#define EXO_PCM_FORMATS_CASE(F)                                                \
    case exo::PcmSampleFormat::F:                                              \
        return exo::uninterleaveToFloat_<exo::PcmSampleFormat::F>(             \
            l, r, src, channels == 1, frames);
        EXO_PCM_FORMATS_SWITCH
#undef EXO_PCM_FORMATS_CASE
    default:
        EXO_UNREACHABLE;
    }
}

void Mp3Encoder::pcmBlock(std::size_t frameCount,
                          std::span<const exo::byte> data) {
    if (!init_)
        return;

    auto lame = lame_.get();
    granule_ += frameCount;

    constexpr std::size_t fitFrames = 256;
    std::array<float, fitFrames> left, right;
    const byte* src = data.data();
    std::size_t count = data.size() / pcmFormat_.bytesPerFrame();
    alignas(std::uintmax_t)
        exo::byte alignedBuffer[fitFrames * exo::MAX_BYTES_PER_FRAME] = {0};
    const auto pcmFormat = pcmFormat_;
    buffer_.reserve(7200 + (fitFrames * 5) / 4);

    while (count > 0) {
        std::size_t frames = std::min(count, fitFrames);
        std::size_t bytes = frames * pcmFormat_.bytesPerFrame();

        // copy sample data to aligned buffer
        std::copy(src, src + bytes, alignedBuffer);
        src += bytes, count -= frames;

        // convert and submit samples
        exo::uninterleaveToFloat(pcmFormat, left.data(), right.data(),
                                 alignedBuffer, frames);

        int ret = lame_encode_buffer_ieee_float(
            lame, left.data(), right.data(), static_cast<int>(frames),
            buffer_.data(), buffer_.capacity());
        if (ret < 0) {
            EXO_LAME_ERROR("lame_encode_buffer_ieee_float", ret);
            init_ = false;
            return;
        }

        granule_ += frames;
        if (ret > 0) {
            packet(granule_ - lame_get_mf_samples_to_encode(lame),
                   {buffer_.data(), static_cast<std::size_t>(ret)});
        }
    }
}

void Mp3Encoder::endTrack() {
    auto lame = lame_.get();
    buffer_.reserve(7200);
    for (;;) {
        int ret = lame_encode_flush(lame, buffer_.data(), buffer_.capacity());
        if (ret < 0) {
            EXO_LAME_ERROR("lame_encode_flush", ret);
            break;
        }
        if (ret == 0)
            break;
        packet(granule_ - lame_get_mf_samples_to_encode(lame),
               {buffer_.data(), static_cast<std::size_t>(ret)});
    }
    init_ = false;
}

} // namespace exo
