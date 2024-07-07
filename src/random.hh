/***
exocaster -- audio streaming helper
random.hh -- random number generation, e.g. for dithering

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

#ifndef RANDOM_HH
#define RANDOM_HH

#include <limits>
#include <random>

namespace exo {

template <typename T>
requires (std::is_integral_v<T>)
T engineSeed_() {
    std::random_device rdev;
    using S = std::make_unsigned_t<decltype(rdev())>;
    using U = std::make_unsigned_t<T>;

    static_assert(std::numeric_limits<S>::radix == 2);
    constexpr auto sDigits = std::numeric_limits<S>::digits;

    static_assert(std::numeric_limits<U>::radix == 2);
    constexpr auto uDigits = std::numeric_limits<U>::digits;

    U v = 0;
    for (unsigned dig = 0; dig < uDigits; dig += sDigits)
        v ^= static_cast<U>(static_cast<S>(rdev())) << dig;

    return static_cast<T>(v);
}

template <typename F = float, typename T = std::default_random_engine>
class RandomFloatGenerator {
    T engine_;

public:
    inline RandomFloatGenerator():
            engine_(exo::engineSeed_<typename T::result_type>()) { }

    F operator()() noexcept {
        return std::uniform_real_distribution<F>(F{0}, F{1})(engine_);
    }
};

} // namespace exo

#endif /* RANDOM_HH */
