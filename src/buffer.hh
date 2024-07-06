/***
exocaster -- audio streaming helper
buffer.hh -- concurrent ring buffer

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

#ifndef BUFFER_HH
#define BUFFER_HH

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <utility>
#include <vector>

namespace exo {

template <bool move, typename InputIt, typename OutputIt>
void moveOrCopy_(InputIt start, InputIt end, OutputIt destination) {
    if constexpr (move)
        std::move(start, end, destination);
    else
        std::copy(start, end, destination);
}

template <typename T>
class RingBuffer {
    std::vector<T> buffer_;
    std::size_t size_, head_, tail_;
    mutable std::timed_mutex mutex_;
    std::condition_variable_any waitToRead_, waitToWrite_;
    bool closed_{false};

    bool canRead_() const noexcept {
        return head_ != tail_;
    }

    std::size_t lockedToRead_() const noexcept {
        if (head_ < tail_)
            return head_ - tail_ + buffer_.size();
        else
            return head_ - tail_;
    }

    template <typename OutputIt>
    std::pair<std::size_t, OutputIt> lockedMoveFromBuffer_(
                        OutputIt d_first, std::size_t count) {
        auto dst = d_first;
        std::size_t totalRead = std::min(count, lockedToRead_());

        if (totalRead) {
            std::size_t sliver = buffer_.size() - tail_;
            if (totalRead <= sliver) {
                dst = std::move(buffer_.begin() + tail_,
                                buffer_.begin() + tail_ + totalRead, dst);
                tail_ = totalRead == sliver ? 0 : tail_ + totalRead;

            } else {
                dst = std::move(buffer_.begin() + tail_,
                                buffer_.end(), dst);
                tail_ = totalRead - sliver;
                dst = std::move(buffer_.begin(),
                                buffer_.begin() + tail_, dst);
            }
        }

        return { totalRead, dst };
    }

    bool canWrite_() const noexcept {
        return (head_ + 1) % buffer_.size() != tail_;
    }

    std::size_t lockedToWrite_() const noexcept {
        return size_ - lockedToRead_();
    }
    
    template <typename InputIt, bool move>
    std::pair<std::size_t, InputIt> lockedXferToBuffer_(
                        InputIt first, std::size_t count) {
        auto src = first;
        std::size_t totalWrite = std::min(count, lockedToWrite_());
        
        if (totalWrite) {
            std::size_t sliver = buffer_.size() - head_;
            auto end = src + totalWrite;
            if (totalWrite <= sliver) {
                exo::moveOrCopy_<move>(src, src + totalWrite,
                                       buffer_.begin() + head_);
                head_ = totalWrite == sliver ? 0 : head_ + totalWrite;
            } else {
                auto pivot = src + sliver;
                exo::moveOrCopy_<move>(src, pivot, buffer_.begin() + head_);
                exo::moveOrCopy_<move>(pivot, end, buffer_.begin());
                head_ = totalWrite - sliver;
            }
            src = end;
        }

        return { totalWrite, src };
    }
    
    template <typename InputIt>
    std::pair<std::size_t, InputIt> lockedCopyToBuffer_(
                        InputIt first, std::size_t count) {
        return lockedXferToBuffer_<InputIt, false>(first, count);
    }
    
    template <typename InputIt>
    std::pair<std::size_t, InputIt> lockedMoveToBuffer_(
                        InputIt first, std::size_t count) {
        return lockedXferToBuffer_<InputIt, true>(first, count);
    }

public:
    RingBuffer(std::size_t size) : buffer_(size + 1),
                size_(size), head_(0), tail_(0) {
        buffer_.shrink_to_fit();
    }

    /** Returns the number of elements that fit in this buffer. */
    std::size_t size() const noexcept {
        return size_;
    }

    /** Returns an approximation of the number of elements that can
        be read right now without blocking. */
    std::size_t toRead() const {
        std::lock_guard lock(mutex_);
        return lockedToRead_();
    }

    /** Reads elements into the iterator. Returns the number of elements
        read. This read is non-blocking.
        
        If there were not enough values, the remaining values
        are not affected. */
    template <typename OutputIt>
    std::size_t readPartial(OutputIt d_first, std::size_t count) {
        std::lock_guard lock(mutex_);
        return std::get<0>(lockedMoveFromBuffer_(d_first, count));
    }

    /** Reads elements into the iterator. Returns the number of elements
        read. This read is blocking if there are no elements, but otherwise
        will not block to read the buffer until full.
        
        If there were not enough values, the remaining values
        are not affected. */
    template <typename OutputIt>
    std::size_t readSome(OutputIt d_first, std::size_t count) {
        std::unique_lock lock(mutex_, std::defer_lock);
        auto dst = d_first;
        std::size_t n = 0;
        while (!n) {
            lock.lock();
            waitToRead_.wait(lock, [this]{ return canRead_() || closed_; });
            if (closed_ && !canRead_()) break;
            std::tie(n, dst) = lockedMoveFromBuffer_(dst, count);
            lock.unlock();
            waitToWrite_.notify_one();
        }
        return n;
    }

    /** Reads elements into the iterator. Returns the number of elements
        read. This read is blocking until the buffer has been fully read.
        It may return early and return less than requested only if the
        buffer is closed.
        
        If there were not enough values, the remaining values
        are not affected. */
    template <typename OutputIt>
    std::size_t readFull(OutputIt d_first, std::size_t count) {
        std::unique_lock lock(mutex_, std::defer_lock);
        auto dst = d_first;
        std::size_t n;
        std::size_t originalCount = count;
        while (count > 0) {
            lock.lock();
            waitToRead_.wait(lock, [this]{ return canRead_() || closed_; });
            if (closed_ && !canRead_()) break;
            std::tie(n, dst) = lockedMoveFromBuffer_(dst, count);
            count -= n;
            lock.unlock();
            waitToWrite_.notify_one();
        }
        return originalCount - count;
    }

    /** Reads one element from the buffer, blocking until it is read.
        Returns the element read, or an empty value if the queue was
        closed and no more elements are thus forthcoming. */
    std::optional<T> get() {
        std::unique_lock lock(mutex_, std::defer_lock);
        lock.lock();
        waitToRead_.wait(lock, [this]{ return canRead_() || closed_; });
        if (closed_ && !canRead_()) return { };
        T value(std::move(buffer_[tail_]));
        tail_ = (tail_ + 1) % buffer_.size();
        lock.unlock();
        waitToWrite_.notify_one();
        return { std::move(value) };
    }

    /** Returns an approximation of the number of elements that can
        be written right now without blocking. */
    std::size_t toWrite() const {
        std::lock_guard lock(mutex_);
        return lockedToWrite_();
    }

    /** Writes elements by copying from the iterator. This write is
        non-blocking. Returns the number of elements actually written. */
    template <typename InputIt>
    std::size_t writePartial(InputIt first, std::size_t count) {
        std::unique_lock lock(mutex_);
        auto n = std::get<0>(lockedCopyToBuffer_(first, count));
        lock.unlock();
        if (n) waitToRead_.notify_one();
        return n > 0;
    }

    /** Writes elements by copying from the iterator. This write is
        blocking, and will write the entire buffer, unless the buffer
        is closed during the write. */
    template <typename InputIt>
    void writeFull(InputIt first, std::size_t count) {
        std::unique_lock lock(mutex_, std::defer_lock);
        auto src = first;
        std::size_t n;
        while (count > 0) {
            lock.lock();
            waitToWrite_.wait(lock, [this]{ return canWrite_() || closed_; });
            if (closed_) return;
            std::tie(n, src) = lockedCopyToBuffer_(src, count);
            count -= n;
            lock.unlock();
            waitToRead_.notify_one();
        }
    }

    /** Writes elements by copying from the iterator. This write is
        non-blocking, but tries to write all elements within the given
        timeout. Returns the number of elements actually written. */
    template <typename InputIt>
    std::size_t writeTimed(InputIt first, std::size_t count,
                           double timeoutSeconds) {
        std::unique_lock lock(mutex_, std::defer_lock);
        auto timeNow = std::chrono::system_clock::now();
        auto timeout = timeNow + std::chrono::milliseconds(
                        static_cast<long>(timeoutSeconds * 1000));
        auto src = first;
        std::size_t originalCount = count, n;
        while (count > 0) {
            if (!lock.try_lock_until(timeout)) break;
            if (!waitToWrite_.wait_until(lock, timeout,
                                        [this]{ 
                                            return canWrite_() || closed_;
                                        }))
                break;
            if (closed_) break;
            std::tie(n, src) = lockedCopyToBuffer_(src, count);
            count -= n;
            lock.unlock();
            waitToRead_.notify_one();
        }
        return originalCount - count;
    }

    /** Writes elements by moving from the iterator. This write is
        non-blocking. Returns the number of elements actually written.
        
        If not all values were written, the remaining values
        are not affected. */
    template <typename InputIt>
    std::size_t writeMovePartial(InputIt first, std::size_t count) {
        std::unique_lock lock(mutex_);
        auto n = std::get<0>(lockedMoveToBuffer_(first, count));
        lock.unlock();
        if (n) waitToRead_.notify_one();
        return n > 0;
    }

    /** Writes elements by moving from the iterator. This write is
        blocking, and will write the entire buffer, unless the buffer
        is closed during the write.
        
        If there were not enough values, the remaining values
        are discarded. */
    template <typename InputIt>
    void writeMoveFull(InputIt first, std::size_t count) {
        std::unique_lock lock(mutex_, std::defer_lock);
        auto src = first;
        std::size_t n;
        while (count > 0) {
            lock.lock();
            waitToWrite_.wait(lock, [this]{ return canWrite_() || closed_; });
            if (closed_) break;
            std::tie(n, src) = lockedMoveToBuffer_(src, count);
            count -= n;
            lock.unlock();
            waitToRead_.notify_one();
        }

        while (count-- > 0) {
            T temp = std::move(*src++);
        }
    }

    /** Writes a single element into the array by copying it.
        Blocks until the element is written.
        Returns true if the element was written. This can only return
        false if the buffer is closed before the write succeeds. */
    bool put(const T& value) {
        return writeFull(&value, 1) > 0;
    }

    /** Tries ro write a single element into the array by copying it.
        Returns true if the element was written. Returns false if
        the buffer has no space right now, or is closed. */
    bool putNoWait(const T& value) {
        return writePartial(&value, 1) > 0;
    }

    /** Writes a single element into the array by moving it.
        Blocks until the element is written. The value is discarded
        if not written. */
    void putMove(T&& value) {
        writeMoveFull(&value, 1);
    }

    /** Reads all values from the buffer and discards them,
        effectively clearing it. */
    void clear() {
        {
            std::lock_guard lock(mutex_);
            for (auto& it: buffer_) it = std::move(T{});
            head_ = tail_ = 0;
        }
        waitToWrite_.notify_one();
    }

    /** Returns true if the queue has been closed and there will no
        longer be any values to read. */
    bool closed() noexcept {
        return closedForReads();
    }

    /** Returns true if the queue has been closed and there will no
        longer be any values to read. */
    bool closedForReads() noexcept {
        std::lock_guard lock(mutex_);
        return closed_ && head_ == tail_;
    }

    /** Returns true if the queue has been closed and values
        may no longer be written. */
    bool closedForWrites() noexcept {
        std::lock_guard lock(mutex_);
        return closed_;
    }

    /** Closes the buffer. Any remaining values can still be read, but
        any subsequent reads will fail. Any writes after the buffer is
        closed fail. */
    void close() {
        {
            std::lock_guard lock(mutex_);
            closed_ = true;
        }
        waitToWrite_.notify_all();
        waitToRead_.notify_all();
    }
};

} // namespace exo

#endif /* BUFFER_HH */
