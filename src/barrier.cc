/***
exocaster -- audio streaming helper
barrier.cc -- track change sync barrier

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

#include <cstddef>
#include <limits>
#include <thread>

#include "barrier.hh"

namespace exo {

template <std::unsigned_integral T> static bool isAhead_(T lhs, T rhs) {
    constexpr T CROSSOVER =
        std::numeric_limits<T>::max() & ~(std::numeric_limits<T>::max() >> 1);
    T lr = static_cast<T>(lhs - rhs);
    return lr < CROSSOVER;
}

void Barrier::increment_() noexcept {
    std::lock_guard lock(mutex_);
    ++listeners_;
}

void Barrier::decrement_() noexcept {
    std::lock_guard lock(mutex_);
    if (listeners_)
        --listeners_;
    barrier_.notify_all();
}

void Barrier::sync(std::size_t token) noexcept {
    std::unique_lock lock(mutex_);
    if (queued_ == 0) {
        token_ = token;
    } else if (token_ != token) {
        if (exo::isAhead_<std::size_t>(token_, token)) {
            // free all other workers by modifying the token, and then
            // reset the number of queued, waiting workers.
            token_ = token;
            visited_ = queued_ = 0;
            barrier_.notify_all();
        } else {
            // we have fallen behind. skip.
            return;
        }
    }

    if (++queued_ >= listeners_) {
        // free all waiting workers
        barrier_.notify_all();
    } else {
        barrier_.wait(lock, [this, token]() {
            return queued_ >= listeners_ || token != token_;
        });
        // someone else updated this token.
        // this means we've probably fallen behind.
        if (token != token_)
            return;
    }

    // queued_ >= listeners_
    // count the number of workers that are no longer waiting.
    // once they have all left, reset queued worker count to 0
    if (++visited_ >= queued_)
        visited_ = queued_ = 0;
}

void Barrier::free() noexcept {
    std::lock_guard lock(mutex_);
    listeners_ = 0;
    barrier_.notify_all();
}

} // namespace exo
