/***
exocaster -- audio streaming helper
jobqueue.hh -- decoder job queue

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

#ifndef JOBQUEUE_HH
#define JOBQUEUE_HH

#include <memory>
#include <mutex>

#include "buffer.hh"
#include "server.hh"
#include "util.hh"

namespace exo {

template <typename T>
class Job {
public:
    EXO_DEFAULT_NONCOPYABLE_VIRTUAL_DESTRUCTOR(Job)

    inline Job() { }
    virtual void init() = 0;
    virtual void run(T value) = 0;
};

template <typename T>
using QueuedJob = std::unique_ptr<exo::Job<T>>;

template <typename T>
class JobQueue {
    exo::RingBuffer<exo::QueuedJob<T>> jobs_;
    std::mutex runningJob_, initJob_, waitingJob_;
    std::vector<std::thread> threads_;
    T param_;

    void runJobs() {
        std::unique_lock runningLock(runningJob_, std::defer_lock);
        std::unique_lock initLock(initJob_, std::defer_lock);
        std::unique_lock waitingLock(waitingJob_, std::defer_lock);
        while (exo::shouldRun()) {
            waitingLock.lock();
            std::optional<exo::QueuedJob<T>> maybeJob = jobs_.get();
            if (!maybeJob.has_value()) {
                waitingLock.unlock();
                if (jobs_.closed()) return;
                continue;
            }
            auto& job = maybeJob.value();
            initLock.lock();
            waitingLock.unlock();
            job->init();
            runningLock.lock();
            initLock.unlock();
            job->run(param_);
            runningLock.unlock();
        }
    }

public:
    JobQueue(std::size_t size, T param) : jobs_(size), param_(param) { }

    void addJob(std::unique_ptr<exo::Job<T>>&& job) {
        if (job) jobs_.putMove(std::move(job));
    }

    void start(std::size_t threadCount) {
        if (threads_.size()) stop();
        threads_.reserve(threadCount);
        for (std::size_t i = 0; i < threadCount; ++i)
            threads_.emplace_back([this]() { runJobs(); });
    }

    void close() {
        jobs_.close();
    }

    void stop() {
        close();
        for (auto& thread : threads_)
            thread.join();
    }
};

} // namespace exo

#endif /* JOBQUEUE_HH */
