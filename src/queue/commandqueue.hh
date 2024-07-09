/***
exocaster -- audio streaming helper
queue/commandqueue.hh -- command queue

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

#ifndef QUEUE_COMMANDQUEUE_HH
#define QUEUE_COMMANDQUEUE_HH

#include <memory>

#include "queue/queue.hh"

namespace exo {

struct Command {
    std::string cmd;
    exo::ConfigObject param;
    exo::ConfigObject raw;
};

class CommandQueue {
  private:
    std::unique_ptr<exo::BaseReadQueue> below_;

  public:
    inline CommandQueue(std::unique_ptr<exo::BaseReadQueue>&& below)
        : below_(std::move(below)) {}

    exo::Command nextCommand();
    void close();
};

} // namespace exo

#endif /* QUEUE_COMMANDQUEUE_HH */
