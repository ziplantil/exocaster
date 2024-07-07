/***
exocaster -- audio streaming helper
log.hh -- logging helper

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

#ifndef LOG_HH
#define LOG_HH

#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <mutex>

namespace exo {

inline void log(const char* file, std::size_t lineno, const char* fmt, ...) {
    static std::mutex mutex;
    std::lock_guard lock(mutex);
    std::va_list args;
    va_start(args, fmt);
    auto t = std::chrono::duration_cast<
            std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
    std::fprintf(stderr, "[%0.3f %s:%zu]: ", t * 0.001, file, lineno);
    std::vfprintf(stderr, fmt, args);
    std::putc('\n', stderr);
    std::fflush(stderr);
    va_end(args);
}

} // namespace exo

#define EXO_LOG(...) exo::log(__FILE__, __LINE__, __VA_ARGS__)

#endif /* LOG_HH */
