/***
exocaster -- audio streaming helper
unaligned.hh -- unaligned load/store helper

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

#ifndef UNALIGNED_HH
#define UNALIGNED_HH

#include <cstring>
#include <type_traits>

namespace exo {

/** Loads a value of the type T at the aligned pointer P.

    Requires that T is trivial, and that the pointer is either a
    void pointer, a pointer to T, or a pointer to bytes (chars). */
template <typename T, typename P>
    requires(std::is_same_v<P, T> || std::is_void_v<P> || sizeof(P) == 1)
T alignedLoad(const P* ptr) {
    return *reinterpret_cast<const T*>(ptr);
}

/** Loads a value of the type T at the possibly unaligned pointer P.

    Requires that T is trivial, and that the pointer is either a
    void pointer, a pointer to T, or a pointer to bytes (chars). */
template <typename T, typename P>
    requires(std::is_trivial_v<T> &&
             (std::is_same_v<P, T> || std::is_void_v<P> || sizeof(P) == 1))
T unalignedLoad(const P* ptr) {
    T val;
    std::memcpy(&val, ptr, sizeof(val));
    return val;
}

/** Stores a value of the type T at the aligned pointer P.

    Requires that T is trivial, and that the pointer is either a
    void pointer, a pointer to T, or a pointer to bytes (chars). */
template <typename T, typename P>
    requires(std::is_same_v<P, T> || std::is_void_v<P> || sizeof(P) == 1)
void alignedStore(P* ptr, const T& val) {
    *reinterpret_cast<T*>(ptr) = val;
}

/** Stores a value of the type T at the possibly unaligned pointer P.

    Requires that T is trivial, and that the pointer is either a
    void pointer, a pointer to T, or a pointer to bytes (chars). */
template <typename T, typename P>
    requires(std::is_trivial_v<T> &&
             (std::is_same_v<P, T> || std::is_void_v<P> || sizeof(P) == 1))
void unalignedStore(P* ptr, const T& val) {
    std::memcpy(ptr, &val, sizeof(val));
}

} // namespace exo

#endif /* UNALIGNED_HH */
