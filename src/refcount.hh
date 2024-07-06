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

template <typename T>
class GlobalLibrary {
    inline static std::size_t count_{0};
    inline static std::mutex mutex_;

    void callInit_() {
        static_cast<T*>(this)->init();
    }
    void callQuit_() noexcept {
        static_cast<T*>(this)->quit();
    }

public:
    GlobalLibrary() {
        std::lock_guard lock(mutex_);
        if (!count_++) callInit_();
    }
    EXO_DEFAULT_NONCOPYABLE(GlobalLibrary);
    ~GlobalLibrary() noexcept {
        std::lock_guard lock(mutex_);
        if (!--count_) callQuit_();
    }
};

} // namespace exo

#endif /* REFCOUNT_HH */
