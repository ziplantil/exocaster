/***
exocaster -- audio streaming helper
debug.hh -- debugging helpers

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

#ifndef DEBUG_HH
#define DEBUG_HH

#include <csignal>
#include <cstdlib>

#include "log.hh"
#include "util.hh"

namespace exo {

#if NDEBUG
#define EXO_ENABLE_ASSERTS false
#else
#define EXO_ENABLE_ASSERTS true
#endif

#if NDEBUG
#undef EXO_BREAKPOINT
#elif defined(SIGTRAP)
#define EXO_BREAKPOINT std::raise(SIGTRAP)
#endif

#ifdef EXO_BREAKPOINT
#define EXO_BREAKPOINT_OR_ABORT EXO_BREAKPOINT
#else
#define EXO_BREAKPOINT_OR_ABORT std::abort()
#endif

#ifndef EXO_BREAKPOINT
#if !NDEBUG
#define EXO_BREAKPOINT static_assert(false, "EXO_BREAKPOINT not implemented")
#endif
#endif

inline void debugAssert_(const char* file, std::size_t line, bool cond,
                         const char* condStr, const char* msg = nullptr) {
    if constexpr (!EXO_ENABLE_ASSERTS)
        return;
    if (!cond) {
        if (msg)
            exo::log(file, line, "assertion failed (%s): %s", condStr, msg);
        else
            exo::log(file, line, "assertion failed (%s)", condStr);
        EXO_BREAKPOINT_OR_ABORT;
    }
}

} // namespace exo

#if NDEBUG
#define EXO_ASSERT(...)
#else
#define EXO_ASSERT(cond, ...)                                                  \
    exo::debugAssert_(__FILE__, __LINE__, cond,                                \
                      EXO_STRINGIFY(cond) __VA_OPT__(, ) __VA_ARGS__)
#endif

#endif /* DEBUG_HH */
