/***
exocaster -- audio streaming helper
encoder/libvorbis/oggvorbis.hh -- OGG Vorbis encoder using libvorbis

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

#ifndef ENCODER_LIBVORBIS_OGGVORBIS_HH
#define ENCODER_LIBVORBIS_OGGVORBIS_HH

#include "encoder/encoder.hh"
#include "util.hh"

extern "C" {
#include <vorbis/vorbisenc.h>
}

namespace exo {

template <typename T>
struct VorbisStruct {
    T value;

    T* ptr() { return &value; }
    const T* ptr() const { return &value; }
};

struct VorbisInfo: public VorbisStruct<vorbis_info> {
    VorbisInfo();
    ~VorbisInfo() noexcept;
    EXO_DEFAULT_NONMOVABLE(VorbisInfo)
};

struct VorbisDspState: public VorbisStruct<vorbis_dsp_state> {
    VorbisDspState(exo::VorbisInfo& info);
    ~VorbisDspState() noexcept;
    EXO_DEFAULT_NONMOVABLE(VorbisDspState)
};

struct VorbisBlock: public VorbisStruct<vorbis_block> {
    VorbisBlock(exo::VorbisDspState& dspState);
    ~VorbisBlock() noexcept;
    EXO_DEFAULT_NONMOVABLE(VorbisBlock)
};

struct VorbisComment: public VorbisStruct<vorbis_comment> {
    VorbisComment();
    ~VorbisComment() noexcept;
    EXO_DEFAULT_NONMOVABLE(VorbisComment)
};

struct OggStreamState: public VorbisStruct<ogg_stream_state> {
    OggStreamState(int serial);
    ~OggStreamState() noexcept;
    EXO_DEFAULT_NONMOVABLE(OggStreamState)
};

class OggVorbisEncoder: public exo::BaseEncoder {
    std::unique_ptr<exo::VorbisInfo> info_;
    std::unique_ptr<exo::VorbisDspState> dspState_;
    std::unique_ptr<exo::VorbisBlock> block_;
    std::unique_ptr<exo::VorbisComment> comment_;
    std::unique_ptr<exo::OggStreamState> stream_;
    std::uint32_t serial_;
    bool init_{false};
    bool endOfStream_{false};
    std::size_t granulesInPage_{0};
    std::uint_least64_t lastGranulePosition_{0};
    int channels_;
    std::int_least32_t minBitrate_, nomBitrate_, maxBitrate_;

    void pushPage_(const ogg_page& page);
    void flushBuffers_();
    void flushPages_();

public:
    OggVorbisEncoder(const exo::ConfigObject& config,
                     std::shared_ptr<exo::PcmBuffer> source,
                     exo::PcmFormat pcmFormat);

    exo::StreamFormat streamFormat() const noexcept;

    void startTrack(const exo::Metadata& metadata);
    void pcmBlock(std::size_t frameCount, std::span<const exo::byte> data);
    void endTrack();
};

} // namespace exo

#endif /* ENCODER_LIBVORBIS_OGGVORBIS_HH */
