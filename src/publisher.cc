/***
exocaster -- audio streaming helper
publisher.cc -- event publisher

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

#include "log.hh"
#include "publisher.hh"
#include "server.hh"

namespace exo {

void Publisher::addQueue(std::unique_ptr<exo::BaseWriteQueue>&& queue) {
    queues_.push_back(std::make_unique<exo::PublishQueue>(std::move(queue)));
}

static void serializeJson(std::ostream& stream, const nlohmann::json& msg) {
    // << does not allow specifying ensure_ascii
    stream << msg.dump(-1, ' ', true);
}

static bool convertEvent(const exo::PublishedEvent& event,
                         std::ostream& stream) {
    if (std::holds_alternative<exo::CommandAcknowledgeEvent>(event)) {
        auto& e = std::get<exo::CommandAcknowledgeEvent>(event);
        if (!e.command) return false;
        nlohmann::json message;
        message["type"] = "acknowledge";
        if (e.encoderIndex == exo::CommandAcknowledgeEvent::NO_ENCODER) {
            message["source"] = "decoder";
        } else {
            message["source"] = "encoder";
            message["index"] = e.encoderIndex;
        }
        message["command"] = *e.command;
        serializeJson(stream, message);
        return true;
    }
    return false;
}

void PublishQueue::run() {
    while (EXO_LIKELY(exo::shouldRun())) {
        auto event = events_.get();
        if (!event.has_value()) return;

        if (exo::convertEvent(event.value(), queue_->write()))
            queue_->writeLine();
    }
}

void PublishQueue::push(const exo::PublishedEvent& event) {
    events_.putNoWait(event);
}

void PublishQueue::close() {
    events_.close();
}

void Publisher::push_(const exo::PublishedEvent& event) {
    for (auto& queue: queues_)
        queue->push(event);
}

void Publisher::acknowledgeDecoderCommand(
                std::shared_ptr<exo::ConfigObject> command) {
    auto event = exo::CommandAcknowledgeEvent{
        .encoderIndex = exo::CommandAcknowledgeEvent::NO_ENCODER,
        .command = command
    };
    push_(event);
}

void Publisher::acknowledgeEncoderCommand(std::size_t encoderIndex,
                std::shared_ptr<exo::ConfigObject> command) {
    auto event = exo::CommandAcknowledgeEvent{
        .encoderIndex = encoderIndex,
        .command = command
    };
    push_(event);
}

void Publisher::startQueue_(std::unique_ptr<exo::PublishQueue>& queue) {
    exo::PublishQueue& q = *queue;
    threads_.emplace_back([&q]() { q.run(); });
}

void Publisher::start() {
    for (auto& queue: queues_)
        startQueue_(queue);
}

void Publisher::close() {
    for (auto& queue: queues_)
        queue->close();
}

void Publisher::stop() {
    for (auto& thread: threads_)
        thread.join();
    threads_.clear();
}

} // namespace exo
