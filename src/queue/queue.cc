/***
exocaster -- audio streaming helper
queue/queue.cc -- queue framework

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

#include <algorithm>
#include <unordered_map>
#include <vector>

#include "queue/file.hh"
#include "queue/queue.hh"

#if EXO_CURL
#include "queue/curl/curl.hh"
#define RQUEUE_DEFS_CURL RQUEUE_DEF(httpget, exo::HttpGetReadQueue)
#define WQUEUE_DEFS_CURL WQUEUE_DEF(httppost, exo::HttpPostWriteQueue)
#else
#define RQUEUE_DEFS_CURL
#define WQUEUE_DEFS_CURL
#endif

#if EXO_ZEROMQ
#include "queue/zeromq/zeromq.hh"
#define RQUEUE_DEFS_ZEROMQ RQUEUE_DEF(zeromq, exo::ZeroMqReadQueue)
#define WQUEUE_DEFS_ZEROMQ WQUEUE_DEF(zeromq, exo::ZeroMqWriteQueue)
#else
#define RQUEUE_DEFS_ZEROMQ
#define WQUEUE_DEFS_ZEROMQ
#endif

namespace exo {

#define RQUEUE_DEFS                                                            \
    RQUEUE_DEF(file, exo::FileReadQueue)                                       \
    RQUEUE_DEFS_CURL                                                           \
    RQUEUE_DEFS_ZEROMQ

#define WQUEUE_DEFS                                                            \
    WQUEUE_DEF(file, exo::FileWriteQueue)                                      \
    WQUEUE_DEFS_CURL                                                           \
    WQUEUE_DEFS_ZEROMQ

enum class ReadQueueImpl {
#define RQUEUE_DEF(name, T) name,
    RQUEUE_DEFS
#undef RQUEUE_DEF
};

enum class WriteQueueImpl {
#define WQUEUE_DEF(name, T) name,
    WQUEUE_DEFS
#undef WQUEUE_DEF
};

static std::unordered_map<std::string, exo::ReadQueueImpl> readQueues = {
#define RQUEUE_DEF(N, T) {#N, exo::ReadQueueImpl::N},
    RQUEUE_DEFS
#undef RQUEUE_DEF
};

static std::unordered_map<std::string, exo::WriteQueueImpl> writeQueues = {
#define WQUEUE_DEF(N, T) {#N, exo::WriteQueueImpl::N},
    WQUEUE_DEFS
#undef WQUEUE_DEF
};

std::unique_ptr<exo::BaseReadQueue>
createReadQueue(const std::string& type, const exo::ConfigObject& config,
                const std::string& instanceId) {
    auto it = readQueues.find(type);

    if (it != readQueues.end()) {
        switch (it->second) {
#define RQUEUE_DEF(N, T)                                                       \
    case exo::ReadQueueImpl::N:                                                \
        return std::make_unique<T>(config, instanceId);
            RQUEUE_DEFS
#undef RQUEUE_DEF
        }
    }

    throw exo::UnknownQueueError("unknown read queue '" + type + "'");
}

std::unique_ptr<exo::BaseWriteQueue>
createWriteQueue(const std::string& type, const exo::ConfigObject& config,
                 const std::string& instanceId) {
    auto it = writeQueues.find(type);

    if (it != writeQueues.end()) {
        switch (it->second) {
#define WQUEUE_DEF(N, T)                                                       \
    case exo::WriteQueueImpl::N:                                               \
        return std::make_unique<T>(config, instanceId);
            WQUEUE_DEFS
#undef WQUEUE_DEF
        }
    }

    throw exo::UnknownQueueError("unknown write queue '" + type + "'");
}

void printReadQueueOptions(std::ostream& stream) {
    std::vector<std::string> options;
    for (const auto& [key, _] : readQueues)
        options.push_back(key);

    std::sort(options.begin(), options.end());
    for (const auto& key : options)
        std::cout << " " << key;
}

void printWriteQueueOptions(std::ostream& stream) {
    std::vector<std::string> options;
    for (const auto& [key, _] : writeQueues)
        options.push_back(key);

    std::sort(options.begin(), options.end());
    for (const auto& key : options)
        std::cout << " " << key;
}

} // namespace exo
