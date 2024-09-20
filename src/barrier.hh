/***
exocaster -- audio streaming helper
barrier.hh -- track change sync barrier

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

#ifndef BARRIER_HH
#define BARRIER_HH

#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>

#include "util.hh"

namespace exo {

class BarrierHolder;

class Barrier {
  private:
    std::size_t queued_{0};
    std::size_t listeners_{0};
    std::size_t visited_{0};
    std::size_t token_{0};

    std::mutex mutex_;
    std::condition_variable_any barrier_;

    void increment_() noexcept;
    void decrement_() noexcept;

  public:
    inline Barrier() {}
    EXO_DEFAULT_NONMOVABLE_DEFAULT_DESTRUCTOR(Barrier)

    void sync(std::size_t token) noexcept;
    void free() noexcept;

    friend class exo::BarrierHolder;
};

class BarrierHolder {
  private:
    std::shared_ptr<exo::Barrier> ptr_;

  public:
    inline BarrierHolder(const std::shared_ptr<exo::Barrier>& barrierPtr)
        : ptr_(barrierPtr) {
        if (ptr_)
            ptr_->increment_();
    }

    EXO_DEFAULT_NONCOPYABLE(BarrierHolder)
    inline ~BarrierHolder() noexcept {
        if (ptr_)
            ptr_->decrement_();
    }

    inline const std::shared_ptr<exo::Barrier>& pointer() const noexcept {
        return ptr_;
    }
};

} // namespace exo

#endif /* BARRIER_HH */
