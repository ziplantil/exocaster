/***
exocaster -- audio streaming helper
publisher.hh -- event publisher

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

#ifndef PUBLISHER_HH
#define PUBLISHER_HH

#include <memory>
#include <thread>
#include <variant>
#include <vector>

#include "buffer.hh"
#include "config.hh"
#include "queue/queue.hh"

namespace exo {

struct CommandAcknowledgeEvent {
    static constexpr std::size_t NO_ENCODER = static_cast<std::size_t>(-1);

    std::size_t encoderIndex;
    std::shared_ptr<exo::ConfigObject> command;    
};

using PublishedEvent = std::variant<exo::CommandAcknowledgeEvent>;

class PublishQueue {
    static constexpr std::size_t EVENT_BUFFER_SIZE = 8;

    std::unique_ptr<exo::BaseWriteQueue> queue_;
    exo::RingBuffer<exo::PublishedEvent> events_;

public:
    inline PublishQueue(std::unique_ptr<exo::BaseWriteQueue>&& queue)
        : queue_(std::move(queue)), events_(EVENT_BUFFER_SIZE) { }

    void push(const exo::PublishedEvent& event);
    void run();
    void close();
};

class Publisher {
    std::vector<std::unique_ptr<exo::PublishQueue>> queues_;
    std::vector<std::thread> threads_;

    void push_(const exo::PublishedEvent& event);
    void startQueue_(std::unique_ptr<exo::PublishQueue>& queue);

public:
    void addQueue(std::unique_ptr<exo::BaseWriteQueue>&& queue);
    void start();
    void close();
    void stop();

    void acknowledgeDecoderCommand(std::shared_ptr<exo::ConfigObject> command);
    void acknowledgeEncoderCommand(std::size_t encoderIndex,
                                   std::shared_ptr<exo::ConfigObject> command);
};

} // namespace exo

#endif /* PUBLISHER_HH */
