/***
exocaster -- audio streaming helper
fclock.hh -- frame timing clock

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

#ifndef FCLOCK_HH
#define FCLOCK_HH

#include <chrono>
#include <thread>
#include <utility>

namespace exo {

template <typename T = std::chrono::steady_clock>
class FrameClock {
    using TimeUnit = std::uint_least64_t; // in nanoseconds

    // the last time
    TimeUnit lastTime_;
    // the duration of one frame
    TimeUnit frameDuration_;
    // the time remaining for one frame
    TimeUnit frameRemainder_;
    // number of frames
    unsigned long long frames_;

    inline TimeUnit elapsed_() noexcept {
        auto newValue = std::chrono::duration_cast<
                            std::chrono::duration<TimeUnit, std::nano>>(
                                T::now().time_since_epoch()).count();
        auto oldValue = std::exchange(lastTime_, newValue);
        return newValue - oldValue;
    }

public:
    inline FrameClock(unsigned long frameRate) noexcept
            : frameDuration_(TimeUnit(std::nano::den / frameRate)),
              frameRemainder_(0), frames_(0) {
        elapsed_();  // init clock
    }

    inline void reset() {
        frameRemainder_ = 0;
        frames_ = 0;
        elapsed_();
    }

    inline void update(unsigned long gotFrames = 0) noexcept {
        auto elapsed = elapsed_() + frameRemainder_;
        auto elapsedFrames = elapsed / frameDuration_;
        frameRemainder_ = elapsed % frameDuration_;
        frames_ = frames_ + static_cast<long long>(gotFrames) - elapsedFrames;
    }

    inline auto wouldSleepUntil(unsigned long gotFrames = 0) noexcept {
        auto until = T::now();
        auto frames = frames_ + gotFrames;
        if (frames > 0)
            until += std::chrono::nanoseconds(frameDuration_ * frames);
        return until;
    }

    inline void sleepIf(std::size_t atLeastFrames) noexcept {
        while (frames_ < atLeastFrames) {
            auto nanosToSleep = frameDuration_ * (frames_ - atLeastFrames / 2);
            std::this_thread::sleep_for(
                        std::chrono::nanoseconds(nanosToSleep));
        }
        update(0);
    }

};

} // namespace exo

#endif /* FCLOCK_HH */
