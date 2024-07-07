/***
exocaster -- audio streaming helper
encoder/libflac/oggflac.cc -- OGG FLAC encoder using libFLAC

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

#include <random>
#include <stdexcept>

#include "encoder/libflac/oggflac.hh"
#include "config.hh"
#include "log.hh"
#include "pcmconvert.hh"
#include "pcmtypes.hh"
#include "random.hh"
#include "server.hh"
#include "unaligned.hh"

extern "C" {
#include <FLAC/metadata.h>
#include <FLAC/stream_encoder.h>
}

namespace exo {

extern "C" {

static FLAC__StreamEncoderWriteStatus writeFlacSamples_(
                            const FLAC__StreamEncoder *encoder,
                            const FLAC__byte buffer[], std::size_t bytes,
                            std::uint32_t samples,
                            std::uint32_t current_frame, void *client_data) {
    return reinterpret_cast<OggFlacEncoder *>(client_data)->writeCallback(
            buffer, bytes, samples, current_frame);
}

}

static constexpr std::size_t SAMPLE_BLOCK_SIZE = 1024;
static_assert(SAMPLE_BLOCK_SIZE >= exo::MAX_CHANNELS);

static exo::PcmSampleFormat getFlacSampleFormat(exo::PcmSampleFormat fmt) {
    if (exo::areSamplesSignedInt(fmt))
        return fmt;
    if (exo::areSamplesUnsignedInt(fmt)
                || exo::areSamplesFloatingPoint(fmt)) {
        switch (fmt) {
        // signed int types, only here to complete the switch
        case exo::PcmSampleFormat::S8:
        case exo::PcmSampleFormat::S16:
            return fmt;
        case exo::PcmSampleFormat::U8:
            return exo::PcmSampleFormat::S8;
        case exo::PcmSampleFormat::F32:
            return exo::PcmSampleFormat::S16;
        default:
            EXO_UNREACHABLE;
        }
    }
    throw std::runtime_error("unsupported sample format for flac encoder");
}

exo::OggFlacEncoder::OggFlacEncoder(const exo::ConfigObject& config,
                std::shared_ptr<exo::PcmBuffer> source,
                exo::PcmFormat pcmFormat):
                BaseEncoder(source, pcmFormat) {
    encoder_ = FLAC__stream_encoder_new();
    if (!encoder_) throw std::bad_alloc();

    std::random_device dev;
    std::uniform_int_distribution<std::uint32_t> dist(0, UINT32_MAX);
    serial_ = static_cast<std::uint32_t>(dist(dev));
    level_ = cfg::namedUInt<unsigned>(config, "level", 5);
    if (level_ > 8)
        throw std::runtime_error("flac encoder: level out of range [0, 8]");

    switch (pcmFormat.channels) {
    case exo::PcmChannelLayout::Mono:
        channels_ = 1;
        break;
    case exo::PcmChannelLayout::Stereo:
        channels_ = 2;
        break;
    default:
        EXO_UNREACHABLE;
        throw std::runtime_error("flac encoder: unsupported channel layout");
    }
    if (channels_ != exo::channelCount(pcmFormat.channels))
        throw std::runtime_error("flac encoder: unsupported channel layout");

    flacSampleFormat_ = exo::getFlacSampleFormat(pcmFormat.sample);
    if (!exo::areSamplesSignedInt(flacSampleFormat_))
        throw std::runtime_error("internal error: FLAC must take signed int");

    if (pcmFormat.rate > UINT32_MAX)
        throw std::runtime_error("flac encoder: unsupported sample rate");

    pcmBuffer_.resize(SAMPLE_BLOCK_SIZE * pcmFormat.bytesPerSample());
}

exo::OggFlacEncoder::~OggFlacEncoder() noexcept {
    if (encoder_) FLAC__stream_encoder_delete(encoder_);
    if (metadata_) FLAC__metadata_object_delete(metadata_);
}

exo::StreamFormat exo::OggFlacEncoder::streamFormat() const noexcept {
    return exo::EncodedStreamFormat{
        exo::EncodedStreamFormatCodec::OGG_FLAC
    };
}

void exo::OggFlacEncoder::startTrack(const exo::Metadata& metadata) {
    if (init_) endTrack();

    if (!FLAC__stream_encoder_set_verify(encoder_, false)) {
        EXO_LOG("FLAC__stream_encoder_set_verify returned false");
        return;
    }
	if (!FLAC__stream_encoder_set_channels(encoder_, channels_)) {
        EXO_LOG("FLAC__stream_encoder_set_channels returned false");
        return;
    }
	if (!FLAC__stream_encoder_set_bits_per_sample(encoder_,
                CHAR_BIT * exo::bytesPerSampleFormat(flacSampleFormat_))) {
        EXO_LOG("FLAC__stream_encoder_set_bits_per_sample returned false");
        return;
    }
	if (!FLAC__stream_encoder_set_sample_rate(encoder_, pcmFormat_.rate)) {
        EXO_LOG("FLAC__stream_encoder_set_sample_rate returned false");
        return;
    }
    if (!FLAC__stream_encoder_set_ogg_serial_number(encoder_,
                            static_cast<long>(serial_++))) {
        EXO_LOG("FLAC__stream_encoder_set_ogg_serial_number returned false");
        return;
    }
    if (!FLAC__stream_encoder_set_compression_level(encoder_, level_)) {
        EXO_LOG("FLAC__stream_encoder_set_compression_level returned false");
        return;
    }
    if (!FLAC__stream_encoder_set_streamable_subset(encoder_, true)) {
        EXO_LOG("FLAC__stream_encoder_set_streamable_subset returned false");
        return;
    }
    if (!FLAC__stream_encoder_set_total_samples_estimate(encoder_, 0)) {
        EXO_LOG("FLAC__stream_encoder_set_total_samples_estimate "
                "returned false");
        return;
    }
    if (!FLAC__stream_encoder_set_limit_min_bitrate(encoder_, true)) {
        EXO_LOG("FLAC__stream_encoder_set_limit_min_bitrate returned false");
        return;
    }

    // build vorbis comment

    if (metadata_) FLAC__metadata_object_delete(metadata_);
    metadata_ = FLAC__metadata_object_new(FLAC__METADATA_TYPE_VORBIS_COMMENT);
    if (!metadata_) {
        EXO_LOG("FLAC__metadata_object_new failed; skipping Vorbis comment");
    } else {
        for (const auto& [key, value]: metadata) {
            FLAC__StreamMetadata_VorbisComment_Entry entry;
            if (FLAC__metadata_object_vorbiscomment_entry_from_name_value_pair(
                        &entry, key.c_str(), value.c_str()))
                if (!FLAC__metadata_object_vorbiscomment_append_comment(
                        metadata_, entry, false))
                    std::free(entry.entry);
        }

        if (!FLAC__stream_encoder_set_metadata(encoder_, &metadata_, 1)) {
            EXO_LOG("FLAC__stream_encoder_set_metadata returned false");
            return;
        }
    }

    auto status = FLAC__stream_encoder_init_ogg_stream(encoder_, nullptr,
                &exo::writeFlacSamples_, nullptr, nullptr, nullptr, this);
    if (status != FLAC__STREAM_ENCODER_INIT_STATUS_OK) {
        EXO_LOG("FLAC__stream_encoder_init_ogg_stream failed: %s",
                FLAC__StreamEncoderInitStatusString[status]);
        return;
    }

    init_ = true;
}

template <typename T>
struct false_ : std::false_type { };

template <exo::PcmSampleFormat Fdst, exo::PcmSampleFormat Fsrc>
static exo::PcmFormat_t<Fdst> convertSampleToSigned_(const exo::byte*& src) {
    static_assert(exo::IsSampleSignedInt_v<Fdst>);
    using Tsrc = exo::PcmFormat_t<Fsrc>;

    auto value = exo::unalignedLoad<Tsrc>(src);
    src += sizeof(Tsrc);

    if constexpr (exo::IsSampleSignedInt_v<Fsrc>
               || exo::IsSampleUnsignedInt_v<Fsrc>) {
        return exo::convertSampleIntToInt_<Fdst, Fsrc>(value);

    } else if constexpr (exo::IsSampleFloatingPoint_v<Fsrc>) {
        // add random noise and floor sum to dither
        static thread_local exo::RandomFloatGenerator<Tsrc> ditherer;
        return exo::convertSampleFromFloat_<Fdst, false>(value, ditherer());

    } else {
        static_assert(false_<Tsrc>::value);
    }
}

template <exo::PcmSampleFormat Fdst>
static exo::PcmFormat_t<Fdst> convertSampleToSigned(const exo::byte*& src,
                    exo::PcmSampleFormat srcFormat) {
    switch (srcFormat) {
#define EXO_PCM_FORMATS_CASE(F)                                                \
    case exo::PcmSampleFormat::F:                                              \
        return exo::convertSampleToSigned_<Fdst, exo::PcmSampleFormat::F>(src);
    EXO_PCM_FORMATS_SWITCH
#undef EXO_PCM_FORMATS_CASE
        default: EXO_UNREACHABLE;
    }
}

template <exo::PcmSampleFormat F>
static void convertSampleToInt32_(FLAC__int32* dst, const exo::byte* src,
                                  std::size_t samples,
                                  exo::PcmSampleFormat srcFormat) {
    static_assert(exo::IsSampleSignedInt_v<F>);
    using T = exo::PcmFormat_t<F>;

    for (std::size_t i = 0; i < samples; ++i) {
        T converted = exo::convertSampleToSigned<F>(src, srcFormat);
        *dst++ = static_cast<FLAC__int32>(converted);
    }
}

static void convertSampleToInt32(FLAC__int32* dst, const exo::byte* src,
                                 std::size_t samples,
                                 exo::PcmSampleFormat dstFormat,
                                 exo::PcmSampleFormat srcFormat) {
    if (EXO_UNLIKELY(!exo::areSamplesSignedInt(dstFormat)))
        throw std::runtime_error("internal error: "
                                 "dstFormat must be a signed int type");

    switch (dstFormat) {
    case exo::PcmSampleFormat::S8:
        return convertSampleToInt32_<exo::PcmSampleFormat::S8>(
                    dst, src, samples, srcFormat);
    case exo::PcmSampleFormat::S16:
        return convertSampleToInt32_<exo::PcmSampleFormat::S16>(
                    dst, src, samples, srcFormat);
    default:
        throw std::runtime_error("internal error: unsupported dstFormat");
    }
}

void exo::OggFlacEncoder::pcmBlock(std::size_t frameCount,
                                   std::span<const exo::byte> data) {
    if (!init_) return;

    const auto pcmFormat = pcmFormat_;
    std::size_t bytesPerFrame = pcmFormat.bytesPerFrame();

    std::array<FLAC__int32, SAMPLE_BLOCK_SIZE> convBuffer = { 0 };
    std::size_t frames = data.size() / bytesPerFrame;
    std::size_t framesPerBuffer = SAMPLE_BLOCK_SIZE / channels_;
    FLAC__int32* dst = convBuffer.data();
    const exo::byte* src = data.data();

    while (frames > 0 && EXO_LIKELY(exo::shouldRun())) {
        auto framesThisTime = std::min(frames, framesPerBuffer);
        auto samplesThisTime = framesThisTime * channels_;
        auto bytesThisTime = framesThisTime * bytesPerFrame;

        std::copy(src, src + bytesThisTime, pcmBuffer_.data());
        src += bytesThisTime;
        exo::convertSampleToInt32(dst, pcmBuffer_.data(), samplesThisTime,
                                  flacSampleFormat_, pcmFormat.sample);

        if (!FLAC__stream_encoder_process_interleaved(encoder_,
                            convBuffer.data(), framesThisTime)) {
            EXO_LOG("FLAC__stream_encoder_process_interleaved failed: %s",
                    FLAC__StreamEncoderStateString[
                            FLAC__stream_encoder_get_state(encoder_)]);
            init_ = false;
            return;
        }

        frames -= framesThisTime;
    }
}

void exo::OggFlacEncoder::endTrack() {
    if (!init_) return;
    if (!FLAC__stream_encoder_finish(encoder_))
        EXO_LOG("FLAC__stream_encoder_finish returned false");
    init_ = false;
}

FLAC__StreamEncoderWriteStatus exo::OggFlacEncoder::writeCallback(
                    const FLAC__byte buffer[], std::size_t bytes,
                    std::uint32_t samples, std::uint32_t currentFrame) {
    packet(samples, { buffer, bytes });
    return FLAC__STREAM_ENCODER_WRITE_STATUS_OK;
}

} // namespace exo
