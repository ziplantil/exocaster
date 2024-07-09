/***
exocaster -- audio streaming helper
encoder/encoder.hh -- encoder framework

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

#ifndef ENCODER_ENCODER_HH
#define ENCODER_ENCODER_HH

#include <iostream>
#include <memory>

#include "config.hh"
#include "packet.hh"
#include "pcmbuffer.hh"
#include "resampler/resampler.hh"
#include "streamformat.hh"

namespace exo {

class UnknownEncoderError : public std::logic_error {
  public:
    using std::logic_error::logic_error;
};

class BaseEncoder {
    static constexpr std::size_t ENCODER_BUFFER = 4096;

  protected:
    std::shared_ptr<exo::PcmBuffer> source_;
    std::vector<std::shared_ptr<exo::PacketRingBuffer>> sinks_;
    exo::PcmFormat pcmFormat_;
    bool startOfTrack_{false};

    void packet(std::size_t frameCount, std::span<const exo::byte> data);
    void packet(unsigned flags, std::size_t frameCount,
                std::span<const exo::byte> data);

  public:
    /*
    BaseEncoder(const exo::ConfigObject& config,
                std::shared_ptr<exo::PcmBuffer> source,
                exo::PcmFormat pcmFormat,
                const exo::ResamplerFactory& resamplerFactory);
    */
    inline BaseEncoder(std::shared_ptr<exo::PcmBuffer> source,
                       exo::PcmFormat pcmFormat)
        : source_(source), sinks_{}, pcmFormat_(pcmFormat) {}
    EXO_DEFAULT_NONCOPYABLE_VIRTUAL_DESTRUCTOR(BaseEncoder)

    /** Returns the format that this encoder encodes into. */
    virtual exo::StreamFormat streamFormat() const noexcept = 0;

    /** Marks the start of a track, with metadata. */
    virtual void startTrack(const exo::Metadata& metadata) = 0;

    /** Feeds a block of PCM data with the given number of frames
        (samples per channel) to the encoder. */
    virtual void pcmBlock(std::size_t frameCount,
                          std::span<const exo::byte> data) = 0;

    /** Marks the end of a track. Guaranteed to be called once for
        every startTrack, and before startTrack is called again. */
    virtual void endTrack() {}

    inline void addSink(std::shared_ptr<exo::PacketRingBuffer> ptr) {
        sinks_.push_back(ptr);
    }
    void run();
    void close();
};

std::unique_ptr<exo::BaseEncoder>
createEncoder(const std::string& type, const exo::ConfigObject& config,
              std::shared_ptr<exo::PcmBuffer> source, exo::PcmFormat pcmFormat,
              const exo::ResamplerFactory& resamplerFactory);

void printEncoderOptions(std::ostream& stream);

} // namespace exo

#endif /* ENCODER_ENCODER_HH */
