/***
exocaster -- audio streaming helper
slot.hh -- template for holding owned C structs

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

#ifndef SLOT_HH
#define SLOT_HH

#include <type_traits>
#include <utility>

#include "util.hh"

namespace exo {

/** Holds owned C structs by a pointer.

    Use with a CRTP, i.e. uses should be structs inheriting from a
    specialization of this template, and implementing the
    constructor and destructor. */
template <typename S, typename T> class PointerSlot {
  private:
    T* ptr_;

  protected:
    PointerSlot(T* ptr) : ptr_(ptr) {}

  public:
    PointerSlot(const std::nullptr_t&) : ptr_(nullptr) {}

    /** Calls the destructor if the pointer is not null, and then
        sets it to the given pointer. */
    void reset(T* ptr = nullptr) noexcept {
        if (ptr_ != nullptr)
            static_cast<S*>(this)->~S();
        ptr_ = ptr;
    }

    PointerSlot(const PointerSlot&) = delete;
    PointerSlot& operator=(const PointerSlot&) = delete;
    PointerSlot(PointerSlot&& s) : ptr_(std::exchange(s.ptr_, nullptr)) {}
    PointerSlot& operator=(PointerSlot&& s) {
        reset(std::exchange(s.ptr_, nullptr));
        return *this;
    }

    /** Gets the underlying pointer. */
    T* get() noexcept { return ptr_; }
    /** Gets the underlying pointer. */
    const T* get() const noexcept { return ptr_; }
    /** Returns a reference to the pointer for allocation.
        Resets the value to null first if it isn't. */
    T*& set() noexcept {
        reset(nullptr);
        return ptr_;
    }
    /** Returns a reference to the pointer for reallocation.
        Be careful! Does not run constructors or destructors. */
    T*& modify() noexcept { return ptr_; }
    /** Releases the underlying pointer and resets the slot to null. */
    T* release() noexcept { return std::exchange(ptr_, nullptr); }

    /** Checks whether the pointer is not null. */
    bool has() const noexcept { return ptr_ != nullptr; }

    operator bool() const noexcept { return ptr_ != nullptr; }

    // none of these are available if T is void
    std::add_lvalue_reference_t<T> operator*() noexcept
        requires(!std::is_void_v<T>)
    {
        return *ptr_;
    }
    const std::add_lvalue_reference_t<T> operator*() const noexcept
        requires(!std::is_void_v<T>)
    {
        return *ptr_;
    }
    T* operator->() noexcept
        requires(!std::is_void_v<T>)
    {
        return ptr_;
    }
    const T* operator->() const noexcept
        requires(!std::is_void_v<T>)
    {
        return ptr_;
    }
};

/** Holds owned C structs by a value.

    Use with a CRTP, i.e. uses should be structs inheriting from a
    specialization of this template, and implementing the
    constructor and destructor.

    Since this owns the value, it's a good idea to use std::unique_ptr
    with this, e.g. std::unique_ptr<exo::SpecializedValueSlot>. */
template <typename S, typename T> class ValueSlot {
  private:
    T val_;

  protected:
    ValueSlot()
        requires std::is_default_constructible_v<T>
        : val_{} {}
    ValueSlot(T&& val) : val_(std::forward(val)) {}

  public:
    EXO_DEFAULT_NONMOVABLE(ValueSlot)

    T* get() noexcept { return &val_; }
    const T* get() const noexcept { return &val_; }
    T& operator*() noexcept { return val_; }
    const T& operator*() const noexcept { return val_; }
    T* operator->() noexcept { return &val_; }
    const T* operator->() const noexcept { return &val_; }

    ValueSlot<S, T>& exchange(T&& val) noexcept {
        static_cast<S*>(this)->~S();
        val_ = std::move(val);
        return *this;
    }
};

} // namespace exo

#endif /* SLOT_HH */
