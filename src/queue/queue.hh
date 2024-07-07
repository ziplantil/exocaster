/***
exocaster -- audio streaming helper
queue/queue.hh -- queue framework

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

#ifndef QUEUE_QUEUE_HH
#define QUEUE_QUEUE_HH

#include <iostream>
#include <memory>
#include <stdexcept>

#include "config.hh"
#include "util.hh"

namespace exo {

class UnknownQueueError : public std::logic_error {
public:
    using std::logic_error::logic_error;
};

class BaseReadQueue {
public:
    /*
    BaseReadQueue(const exo::ConfigObject& config,
                  const std::string& instanceId);
    */
    inline BaseReadQueue() { }
    EXO_DEFAULT_NONCOPYABLE_VIRTUAL_DESTRUCTOR(BaseReadQueue)

    virtual exo::ConfigObject readLine() = 0;
    virtual void close() { }
};

class BaseWriteQueue {
public:
    /*
    BaseWriteQueue(const exo::ConfigObject& config);
    */
    inline BaseWriteQueue() { }
    EXO_DEFAULT_NONCOPYABLE_VIRTUAL_DESTRUCTOR(BaseWriteQueue)

    virtual std::ostream& write() = 0;
    virtual void writeLine() = 0;
};

std::unique_ptr<exo::BaseReadQueue> createReadQueue(
                const std::string& type,
                const exo::ConfigObject& config,
                const std::string& instanceId);
std::unique_ptr<exo::BaseWriteQueue> createWriteQueue(
                const std::string& type,
                const exo::ConfigObject& config,
                const std::string& instanceId);

void printReadQueueOptions(std::ostream& stream);
void printWriteQueueOptions(std::ostream& stream);

} // namespace exo

#endif /* QUEUE_QUEUE_HH */
