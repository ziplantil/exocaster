/***
exocaster -- audio streaming helper
resampler/types.hh -- resampler types

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

#ifndef RESAMPLER_TYPES_HH
#define RESAMPLER_TYPES_HH

#include <concepts>
#include <cstdint>
#include <span>

#include "config.hh"

namespace exo {

using SampleRate = std::uint_least32_t;

struct ResamplerReturn {
    std::size_t wrote;
    std::size_t read;
};

template <typename T>
concept ResamplerConcept =
    requires(T& x, exo::SampleRate outRate, exo::SampleRate inRate,
             std::span<float> dst, std::span<const float> src,
             const exo::ConfigObject& config) {
        T(outRate, inRate, config);

        /** Resamples floating-point PCM samples given in src to dst.

            Returns the number of samples read from the input and
            written to the output.

            The resampler may not read all samples from the source buffer. This
            may happen if the internal buffer in the resampler is full and
            cannot admit any more samples. Users must check the read field of
            the return value to check how many samples from the source buffer
            were actually read.

            Likewise, if the destination buffer is too small, the resampler may
            not return all samples that it is able to. The resampler will also
            not write any partial samples, i.e. those that still need more data
            to resample properly. These can only be obtained by flushing. */
        { x.resample(dst, src) } -> std::same_as<exo::ResamplerReturn>;

        /** Yields any remaining partial samples from the resampler, with
            the assumption that any additional samples needed to establish
            their final value are silence.

            Returns the number of samples written to the buffer. */
        { x.flush(dst) } -> std::same_as<std::size_t>;
    };

} // namespace exo

#endif /* RESAMPLER_TYPES_HH */
