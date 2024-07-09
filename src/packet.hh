/***
exocaster -- audio streaming helper
packet.hh -- packet system

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

#ifndef PACKET_HH
#define PACKET_HH

#include <cstring>
#include <optional>

#include "buffer.hh"
#include "types.hh"

namespace exo {

namespace PacketFlags {
static constexpr unsigned StartOfTrack = 1;
static constexpr unsigned OutOfBandMetadata = 2;
}; // namespace PacketFlags

struct PacketHeader {
    std::size_t dataSize;
    std::size_t frameCount;
    unsigned flags;
};

template <typename T>
    requires(std::is_trivial_v<T>)
struct RingBufferWithHeader : public exo::RingBuffer<exo::byte> {
    using exo::RingBuffer<exo::byte>::RingBuffer;

    std::optional<T> readHeader() {
        T header;
        exo::byte headerBytes[sizeof(header)];
        auto read = readFull(headerBytes, sizeof(header));
        if (read < sizeof(header))
            return {};
        std::memcpy(&header, headerBytes, sizeof(header));
        return header;
    }

    void writeHeader(const T& header) {
        exo::byte headerBytes[sizeof(header)];
        std::memcpy(headerBytes, &header, sizeof(header));
        writeFull(headerBytes, sizeof(header));
    }
};

struct PacketRingBuffer : public exo::RingBufferWithHeader<exo::PacketHeader> {
  protected:
    using exo::RingBuffer<exo::byte>::get;
    using exo::RingBuffer<exo::byte>::readSome;
    using exo::RingBuffer<exo::byte>::readPartial;
    using exo::RingBuffer<exo::byte>::readFull;
    using exo::RingBuffer<exo::byte>::skipFull;
    using exo::RingBuffer<exo::byte>::put;
    using exo::RingBuffer<exo::byte>::writePartial;
    using exo::RingBuffer<exo::byte>::writeFull;
    using exo::RingBuffer<exo::byte>::writeTimed;

  public:
    using exo::RingBufferWithHeader<exo::PacketHeader>::RingBufferWithHeader;

    class PacketRead {
        PacketRingBuffer* buffer_;
        std::size_t left_;

      public:
        PacketHeader header;

      private:
        inline PacketRead(PacketRingBuffer& buffer,
                          PacketHeader&& header) noexcept
            : buffer_(&buffer), left_(header.dataSize),
              header(std::move(header)) {}

        inline std::size_t wantsToRead_(std::size_t n) const noexcept {
            return n < left_ ? n : left_;
        }

        inline void didRead_(std::size_t n) noexcept {
            left_ = left_ > n ? left_ - n : 0;
        }

      public:
        inline PacketRead() noexcept : buffer_(nullptr), left_(0), header() {}

        bool hasData() const noexcept {
            return left_ > 0 && !buffer_->closed();
        }

        /** Reads bytes from the packet into the iterator.
            Returns the number of bytes read. This read is non-blocking.

            If there were not enough values, the remaining values
            are not affected. */
        template <typename OutputIt>
        std::size_t readPartial(OutputIt d_first, std::size_t count) noexcept {
            count = wantsToRead_(count);
            if (!count)
                return count;
            auto n = buffer_->readPartial(d_first, count);
            didRead_(n);
            return n;
        }

        /** Reads bytes from the packet into the iterator. Returns the number
            of bytes read. This read is blocking if there are no bytes,
            but otherwise will not block to read the buffer until full.

            If there were not enough values, the remaining values
            are not affected. */
        template <typename OutputIt>
        std::size_t readSome(OutputIt d_first, std::size_t count) noexcept {
            count = wantsToRead_(count);
            if (!count)
                return count;
            auto n = buffer_->readSome(d_first, count);
            didRead_(n);
            return n;
        }

        /** Reads bytes from the packet into the iterator. Returns the number
            of bytes read. This read is blocking until the buffer has been
            fully read. It may return early and return less than requested
            only if the packet is exhausted or buffer is closed.

            If there were not enough values, the remaining values
            are not affected. */
        template <typename OutputIt>
        std::size_t readFull(OutputIt d_first, std::size_t count) noexcept {
            count = wantsToRead_(count);
            if (!count)
                return count;
            auto n = buffer_->readFull(d_first, count);
            didRead_(n);
            return n;
        }

        /** Skips this packet. */
        void skipFull() noexcept {
            auto count = left_;
            auto n = buffer_->skipFull(count);
            didRead_(n);
        }

        friend exo::PacketRingBuffer;
    };

    /** Reads a single packet. Can return an empty value only if the
        buffer is closed. */
    inline std::optional<PacketRead> readPacket() {
        auto header = readHeader();
        if (!header.has_value())
            return {};
        return PacketRead(*this, std::move(header.value()));
    }

    /** Writes a single packet. */
    inline void writePacket(unsigned flags, std::size_t frameCount,
                            std::span<const exo::byte> data) {
        writeHeader(exo::PacketHeader{
            .dataSize = data.size(), .frameCount = frameCount, .flags = flags});
        writeFull(data.data(), data.size());
    }

    /** Reads bytes into the iterator. This bypasses the packet system
        and will read the data from them as necessary, ignoring other
        packet headers. A reference to a PacketRead object must be provided
        to keep track of remaining bytes in the current packet.

        Returns the number of bytes read. This read is non-blocking.

        If there were not enough values, the remaining values
        are not affected.

        Out-of-band packets are skipped. */
    template <typename OutputIt>
    std::size_t readDirectPartial(PacketRead& cache, OutputIt d_first,
                                  std::size_t count) {
        while (!cache.hasData()) {
            auto nextPacket = readPacket();
            if (!nextPacket.has_value())
                return 0;
            cache = std::move(nextPacket.value());
            if (cache.header.flags & PacketFlags::OutOfBandMetadata)
                cache.skipFull();
        }
        return cache.readPartial(d_first, count);
    }

    /** Reads bytes into the iterator. This bypasses the packet system
        and will read the data from them as necessary, ignoring other
        packet headers. A reference to a PacketRead object must be provided
        to keep track of remaining bytes in the current packet.

        Returns the number of elements read. This read is blocking if there
        are no bytes, but otherwise will not block to read the buffer
        until full.

        If there were not enough values, the remaining values
        are not affected.

        Out-of-band packets are skipped.  */
    template <typename OutputIt>
    std::size_t readDirectSome(PacketRead& cache, OutputIt d_first,
                               std::size_t count) {
        while (!cache.hasData()) {
            auto nextPacket = readPacket();
            if (!nextPacket.has_value())
                return 0;
            cache = std::move(nextPacket.value());
            if (cache.header.flags & PacketFlags::OutOfBandMetadata)
                cache.skipFull();
        }
        return cache.readSome(d_first, count);
    }

    /** Reads bytes into the iterator. This bypasses the packet system
        and will read the data from them as necessary, ignoring other
        packet headers. A reference to a PacketRead object must be provided
        to keep track of remaining bytes in the current packet.

        Returns the number of bytes read. This read is blocking until the
        buffer has been fully read. It may return early and return less than
        requested only if the buffer is closed.

        If there were not enough values, the remaining values
        are not affected.

        Out-of-band packets are skipped. */
    template <typename OutputIt>
    std::size_t readDirectFull(PacketRead& cache, OutputIt d_first,
                               std::size_t count) {
        std::size_t n = 0;
        auto dst = d_first;
        for (;;) {
            if (cache.header.flags & PacketFlags::OutOfBandMetadata)
                cache.skipFull();
            else {
                std::size_t b = cache.readFull(dst, count);
                n += b, dst += b, count -= b;
                if (!count)
                    break;
            }

            auto nextPacket = readPacket();
            if (!nextPacket.has_value())
                break;

            cache = std::move(nextPacket.value());
        }
        return n;
    }
};

} // namespace exo

#endif /* PACKET_HH */
