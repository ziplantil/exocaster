/***
exocaster -- audio streaming helper
refcount.hh -- reference counting for libraries with global init

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

#ifndef REFCOUNT_HH
#define REFCOUNT_HH

#include <cstdlib>
#include <mutex>

#include "util.hh"

namespace exo {

/** A reference counter class that can be used to manage global initialization
    and cleanup of C libraries. One GlobalLibrary counter exists per type,
    and normally you want to use CRTP (curiously recurring template pattern)
    for this; create a subclass inheriting from this template, and implement
    two functions: init() and quit(), taking no parameters and returning void.
    Throw from init() if initialization fails.

    When an instance of such a class is created, the counter for that class
    is incremented. The init() function is called when the counter is
    incremented from zero. Finally, when the value is destroyed, the
    counter is decremented, and quit() is called if it reaches zero.

    GlobalLibrary instances can be moved, but not copied. */
template <typename T> class GlobalLibrary {
    inline static std::size_t count_{0};
    inline static std::mutex mutex_;

    void callInit_() { static_cast<T*>(this)->init(); }
    void callQuit_() noexcept { static_cast<T*>(this)->quit(); }

  public:
    GlobalLibrary() {
        std::lock_guard lock(mutex_);
        if (!count_++)
            callInit_();
    }
    EXO_DEFAULT_NONCOPYABLE(GlobalLibrary);
    ~GlobalLibrary() noexcept {
        std::lock_guard lock(mutex_);
        if (!--count_)
            callQuit_();
    }
};

} // namespace exo

#endif /* REFCOUNT_HH */
