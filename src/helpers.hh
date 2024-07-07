/***
exocaster -- audio streaming helper
helpers.hh -- common utilities

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

#ifndef HELPERS_HH
#define HELPERS_HH

#include <array>
#include <span>

namespace exo {

template <typename T, std::size_t N>
std::span<T> arrayAsSpan(std::array<T, N>& array) {
    return std::span<T>(array.begin(), array.size());
}

template <typename T, std::size_t N>
std::span<const T> arrayAsSpan(const std::array<T, N>& array) {
    return std::span<const T>(array.begin(), array.size());
}

} // namespace exo

#endif /* HELPERS_HH */
