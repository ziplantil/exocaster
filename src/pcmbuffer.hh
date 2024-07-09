/***
exocaster -- audio streaming helper
pcmbuffer.hh -- PCM and metadata buffer

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

#ifndef PCMBUFFER_HH
#define PCMBUFFER_HH

#include <array>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <span>
#include <vector>

#include "buffer.hh"
#include "config.hh"
#include "fclock.hh"
#include "metadata.hh"
#include "pcmtypes.hh"
#include "publisher.hh"
#include "serverconfig.hh"
#include "types.hh"

namespace exo {

struct PcmBufferRow {
    std::shared_ptr<exo::ConfigObject> command;
    std::shared_ptr<exo::Metadata> metadata;
    std::size_t pcmBytes;
};

static constexpr std::uint_least32_t SKIP_FACTOR_FRAC_BITS = 16;

class PcmBuffer {
    exo::RingBuffer<byte> pcm_;
    // tracks left of this song, i.e. before song change
    std::size_t pcmLeft_{0};
    // each song change: metadata and buffer size after each metadata change
    std::array<exo::PcmBufferRow, 8> songChanges_;
    // head and tail of circular song change bufffer
    std::size_t songHead_{0}, songTail_{0};
    bool closed_{false};
    exo::PcmFormat pcmFormat_;
    std::size_t subindex_;
    std::shared_ptr<exo::Publisher> publisher_;
    bool skip_;
    exo::FrameClock<> frameClock_;
    std::uint_least64_t marginUs_;
    std::uint_least32_t skipFactor_;
    bool firstPcm_{true};

    std::timed_mutex mutex_;
    std::condition_variable_any hasPcm_;

    // whether one has to wait to write next song change
    inline bool metadataFull_() const noexcept {
        return (songHead_ + 1) % songChanges_.size() == songTail_;
    }

  public:
    PcmBuffer(std::size_t subindex, const exo::PcmFormat& pcmFormat,
              std::size_t bufferSize, std::shared_ptr<exo::Publisher> publisher,
              const exo::PcmBufferConfig& config);

    std::shared_ptr<exo::Metadata> readMetadata();
    std::size_t readPcm(std::span<exo::byte> destination);

    void writeMetadata(std::shared_ptr<exo::ConfigObject> command,
                       std::shared_ptr<exo::Metadata> metadata);
    void writePcm(std::span<const exo::byte> data);

    void close() noexcept;
    bool closed() const noexcept;
};

class PcmSplitter {
    std::vector<std::shared_ptr<exo::PcmBuffer>> buffers_;
    exo::PcmFormat pcmFormat_;
    std::size_t bufferSize_;
    std::size_t bufferIndex_{0};
    std::shared_ptr<exo::Publisher> publisher_;
    std::size_t chop_;

  public:
    PcmSplitter(const exo::PcmFormat& pcmFormat, std::size_t bufferSize,
                std::shared_ptr<exo::Publisher> publisher);

    std::shared_ptr<exo::PcmBuffer>
    addBuffer(const exo::PcmBufferConfig& config);
    void metadata(std::shared_ptr<exo::ConfigObject> command,
                  const exo::Metadata& metadata);
    void pcm(std::span<const exo::byte> data);

    inline void skipIndex() { ++bufferIndex_; }

    inline void close() {
        for (auto& buffer : buffers_)
            buffer->close();
        buffers_.clear();
    }
};

} // namespace exo

#endif /* PCMBUFFER_HH */
