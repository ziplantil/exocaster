/***
exocaster -- audio streaming helper
uuid.hh -- UUID class

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

#ifndef UUID_HH
#define UUID_HH

#include <chrono>
#include <cstdio>
#include <random>
#include <ratio>
#include <string>

#include "types.hh"

namespace exo {

/** A UUID (universally unique identifier), consisting of 128 bits of data. */
struct UUID {
    std::array<exo::byte, 16> data;

    /** Converts this UUID to a string using the standard format. */
    inline std::string str() const {
        char buf[] = "00000000-0000-0000-0000-000000000000";
        std::snprintf(buf, sizeof(buf),
                      "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-"
                      "%02x%02x%02x%02x%02x%02x",
                      data[0], data[1], data[2], data[3], data[4], data[5],
                      data[6], data[7], data[8], data[9], data[10], data[11],
                      data[12], data[13], data[14], data[15]);
        return std::string(buf);
    }

    /** Generates a version 7 UUID using the given randomizer. */
    template <typename R> static UUID uuid7(R& randomizer) {
        auto t = std::chrono::system_clock::now();
        auto x = std::chrono::duration_cast<
                     std::chrono::duration<std::uint_least64_t, std::milli>>(
                     t.time_since_epoch())
                     .count();
        std::uniform_int_distribution<exo::byte> dist(0, 255);
        std::array<exo::byte, 16> b;

        for (std::size_t i = 0; i < 6; ++i)
            b[i] = static_cast<exo::byte>((x >> (8 * (5 - i))) & 0xFFU);
        b[6] = 0x70U | (dist(randomizer) & 0x0FU);
        b[7] = dist(randomizer);
        b[8] = 0x80U | (dist(randomizer) & 0x3FU);
        for (std::size_t i = 9; i < 16; ++i)
            b[i] = dist(randomizer);

        return UUID{.data = b};
    }

    /** Generates a version 7 UUID. */
    static UUID uuid7() {
        static std::random_device dev;
        return uuid7(dev);
    }
};

} // namespace exo

#endif /* UUID_HH */
