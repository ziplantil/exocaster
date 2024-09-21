/***
exocaster -- audio streaming helper
metadata.cc -- metadata types

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

#include <cstring>
#include <sstream>
#include <string_view>

#include "metadata.hh"
#include "packetstream.hh"
#include "types.hh"

namespace exo {

/** Generates a std::string for out-of-band metadata. */
std::string writePacketMetadata(const exo::Metadata& metadata) {
    std::ostringstream metadataJoined;
    // header
    metadataJoined << "OOBM";
    for (const auto& [key, value] : metadata) {
        metadataJoined << key << '=' << value << '\0';
    }
    return metadataJoined.str();
}

/** Reads out-of-band metadata from a packet. */
exo::Metadata readPacketMetadata(exo::PacketRingBuffer::PacketRead& packet) {
    exo::byte buffer[256];
    if (packet.readFull(buffer, 4) < 4)
        return {}; // header incomplete
    if (std::memcmp(buffer, "OOBM", 4))
        return {}; // header mismatch

    exo::Metadata meta = {};
    std::ostringstream stream;
    std::string key;
    char scan = '=';

    for (;;) {
        std::size_t n = packet.readFull(buffer, 256);
        if (!n)
            break;

        exo::byte* s = &buffer[0];
        exo::byte* e = &buffer[n];
        while (s < e) {
            exo::byte* p =
                reinterpret_cast<exo::byte*>(std::memchr(s, scan, e - s));
            if (!p) {
                stream << std::string_view(reinterpret_cast<const char*>(s),
                                           e - s);
                break;
            }

            // *p == scan
            stream << std::string_view(reinterpret_cast<const char*>(s), p - s);
            if (scan) {
                key = std::move(stream).str();
                scan = 0;
            } else {
                meta.push_back({std::move(key), std::move(stream).str()});
                scan = '=';
            }
            s = p + 1;
        }
    }

    return meta;
}

std::string
writePacketCommand(const std::shared_ptr<exo::ConfigObject>& metadata) {
    std::ostringstream metadataJoined;
    // header
    metadataJoined << "OOBC";
    metadataJoined << *metadata;
    return metadataJoined.str();
}

std::shared_ptr<exo::ConfigObject>
readPacketCommand(exo::PacketRingBuffer::PacketRead& packet) {
    exo::byte buffer[4];
    if (packet.readFull(buffer, 4) < 4)
        return {}; // header incomplete
    if (std::memcmp(buffer, "OOBC", 4))
        return {}; // header mismatch

    auto config = std::make_shared<exo::ConfigObject>();
    exo::PacketInputStream stream(packet);
    stream >> *config;
    return config;
}

} // namespace exo
