/***
exocaster -- audio streaming helper
queue/zeromq/zeromq.hh -- zeromq queue

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

#ifndef QUEUE_ZEROMQ_ZEROMQ_HH
#define QUEUE_ZEROMQ_ZEROMQ_HH

#include <sstream>

#include <zmq.hpp>

#include "queue/queue.hh"

namespace exo {

class ZeroMqReadQueue: public BaseReadQueue {
    zmq::context_t ctx_;
    zmq::socket_t sock_;
    bool closed_{false};

public:
    ZeroMqReadQueue(const exo::ConfigObject& config,
                    const std::string& instanceId);
    
    exo::ConfigObject readLine();
    void close();
};

class ZeroMqWriteQueue: public BaseWriteQueue {
    zmq::context_t ctx_;
    zmq::socket_t sock_;
    std::ostringstream buffer_;
    std::optional<std::string> topic_;

public:
    ZeroMqWriteQueue(const exo::ConfigObject& config,
                     const std::string& instanceId);
    
    std::ostream& write();
    void writeLine();
};

} // namespace exo

#endif /* QUEUE_ZEROMQ_ZEROMQ_HH */
