/***
exocaster -- audio streaming helper
util.hh -- common compiler utilities

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

#ifndef UTIL_HH
#define UTIL_HH

#if __cpp_lib_unreachable
#include <utility>
#define EXO_UNREACHABLE std::unreachable()
#elif defined(_MSC_VER)
#define EXO_UNREACHABLE __assume(false)
#elif __GNUC__
#define EXO_UNREACHABLE __builtin_unreachable()
#else
#include <exception>
[[noreturn]] inline void unreachable_() {
    std::terminate();
}

#define EXO_UNREACHABLE exo::unreachable_()
#endif

namespace exo {

#define EXO_DEFAULT_COPYABLE(T)                                                \
    T(const T&) = default;                                                     \
    T& operator=(const T&) = default;                                          \
    T(T&&) = default;                                                          \
    T& operator=(T&&) = default;

#define EXO_DEFAULT_NONCOPYABLE(T)                                             \
    T(const T&) = delete;                                                      \
    T& operator=(const T&) = delete;                                           \
    T(T&&) = default;                                                          \
    T& operator=(T&&) = default; 

#define EXO_DEFAULT_NONMOVABLE(T)                                              \
    T(const T&) = delete;                                                      \
    T& operator=(const T&) = delete;                                           \
    T(T&&) = delete;                                                           \
    T& operator=(T&&) = delete; 

#define EXO_DEFAULT_COPYABLE_DEFAULT_DESTRUCTOR(T)                             \
    T(const T&) = default;                                                     \
    T& operator=(const T&) = default;                                          \
    T(T&&) = default;                                                          \
    T& operator=(T&&) = default;                                               \
    ~T() = default;

#define EXO_DEFAULT_NONCOPYABLE_DEFAULT_DESTRUCTOR(T)                          \
    T(const T&) = delete;                                                      \
    T& operator=(const T&) = delete;                                           \
    T(T&&) = default;                                                          \
    T& operator=(T&&) = default;                                               \
    ~T() = default;

#define EXO_DEFAULT_COPYABLE_VIRTUAL_DESTRUCTOR(T)                             \
    T(const T&) = default;                                                     \
    T& operator=(const T&) = default;                                          \
    T(T&&) = default;                                                          \
    T& operator=(T&&) = default;                                               \
    virtual ~T() = default;

#define EXO_DEFAULT_NONCOPYABLE_VIRTUAL_DESTRUCTOR(T)                          \
    T(const T&) = delete;                                                      \
    T& operator=(const T&) = delete;                                           \
    T(T&&) = default;                                                          \
    T& operator=(T&&) = default;                                               \
    virtual ~T() = default;

#if __GNUC__ >= 3
#define EXO_LIKELY(cond) (__builtin_expect(!!(cond), 1))
#define EXO_UNLIKELY(cond) (__builtin_expect(!!(cond), 0))
#else
#define EXO_LIKELY(cond) (!!(cond))
#define EXO_UNLIKELY(cond) (!!(cond))
#endif

} // namespace exo

#endif /* UTIL_HH */
