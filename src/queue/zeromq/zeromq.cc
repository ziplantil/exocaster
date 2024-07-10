/***
exocaster -- audio streaming helper
queue/zeromq/zeromq.cc -- zeromq queue

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

#include <chrono>
#include <exception>
#include <stdexcept>
#include <thread>
#include <utility>

#include "log.hh"
#include "queue/zeromq/zeromq.hh"
#include "server.hh"

#include <zmq.hpp>

namespace exo {

exo::ZeroMqReadQueue::ZeroMqReadQueue(const exo::ConfigObject& config,
                                      const std::string& instanceId)
    : sock_(ctx_, zmq::socket_type::pull) {
    std::string address;
    if (cfg::isObject(config)) {
        if (!cfg::hasString(config, "address"))
            throw std::runtime_error("zeromq config needs 'address'");
        address = cfg::namedString(config, "address");
    } else {
        address = cfg::getString(config);
    }
    sock_.connect(address);
}

exo::ConfigObject exo::ZeroMqReadQueue::readLine() {
    zmq::message_t msg;

    while (exo::acceptsCommands()) {
        auto res = sock_.recv(msg, zmq::recv_flags::none);
        if (!res.has_value()) {
            if (closed_)
                break;
            std::this_thread::sleep_for(std::chrono::seconds(1));
            return {};
        }

        auto view = msg.to_string_view();
        try {
            return cfg::parseFromMemory(view.begin(), view.end());
        } catch (const std::exception& e) {
            EXO_LOG("could not parse message as JSON, ignoring: %s", e.what());
            continue;
        }
    }

    exo::noMoreCommands();
    return {};
}

void exo::ZeroMqReadQueue::close() {
    closed_ = true;
    sock_.close();
}

exo::ZeroMqWriteQueue::ZeroMqWriteQueue(const exo::ConfigObject& config,
                                        const std::string& instanceId)
    : sock_(ctx_, zmq::socket_type::pub) {
    std::string address;
    if (cfg::isObject(config)) {
        if (!cfg::hasString(config, "address"))
            throw std::runtime_error("zeromq config needs 'address'");
        address = cfg::namedString(config, "address");
        if (cfg::hasString(config, "topic"))
            topic_ = cfg::getString(config, "topic");
        if (topic_.has_value() && cfg::hasBoolean(config, "topicId") &&
            cfg::namedBoolean(config, "topicId"))
            topic_.value() += instanceId;
    } else {
        address = cfg::getString(config);
    }
    sock_.bind(address);
}

std::ostream& exo::ZeroMqWriteQueue::write() { return buffer_; }

void exo::ZeroMqWriteQueue::writeLine() {
    auto str = std::move(buffer_).str();
    if (topic_.has_value()) {
        zmq::message_t topicMsg(topic_.value());
        sock_.send(topicMsg, zmq::send_flags::sndmore);
    }
    zmq::message_t msg(str);
    sock_.send(msg, zmq::send_flags::dontwait);
}

} // namespace exo
