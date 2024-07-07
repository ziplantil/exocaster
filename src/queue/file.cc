/***
exocaster -- audio streaming helper
queue/file.cc -- file queue

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

#include "config.hh"
#include "queue/file.hh"
#include "log.hh"
#include "queue/queue.hh"
#include "queue/queueutil.hh"
#include "server.hh"
#include <system_error>

namespace exo {

exo::FileReadQueue::FileReadQueue(const exo::ConfigObject& config,
                                  const std::string& instanceId)
        : exo::BaseReadQueue() {
    std::string path;

    if (cfg::isString(config)) {
        path = cfg::getString(config);
    } else {
        if (!cfg::hasString(config, "file"))
            throw exo::InvalidConfigError("'file' queue config needs 'file'");
        path = cfg::namedString(config, "file");
    }

    file_.exceptions();
    file_.open(path, std::ios::in);
    if (file_.fail() || file_.bad())
        throw std::system_error(errno, std::generic_category(),
                    "file queue error: " + path);
}

exo::ConfigObject exo::FileReadQueue::readLine() {
    while (exo::acceptsCommands()) {
        auto stream = exo::LineInputStream(file_);
        if (file_.eof()) {
            EXO_LOG("file ran out of commands, "
                    "will exit after remaining commands are done");
            exo::noMoreCommands();
            return {};
        }
        if (file_.bad()) {
            EXO_LOG("file read error, cannot continue, "
                    "will exit after remaining commands are done");
            exo::noMoreCommands();
            return {};
        }

        try {
            return cfg::parseFromFile(stream);
        } catch (const std::exception& e) {
            EXO_LOG("could not parse incoming line as JSON, "
                    "ignoring: %s", e.what());
            continue;
        }
    }

    exo::noMoreCommands();
    return {};
}

void exo::FileReadQueue::close() {
    file_.close();
}

exo::FileWriteQueue::FileWriteQueue(const exo::ConfigObject& config,
                                    const std::string& instanceId)
        : exo::BaseWriteQueue() {
    std::string path;
    auto flags = std::ios::out;

    if (cfg::isString(config)) {
        path = cfg::getString(config);
    } else {
        if (!cfg::hasString(config, "file"))
            throw exo::InvalidConfigError("'file' queue config needs 'file'");
        path = cfg::namedString(config, "file");
        if (cfg::namedBoolean(config, "append", false))
            flags |= std::ios::app;
    }

    file_.exceptions(std::ifstream::badbit | std::ifstream::failbit);
    file_.open(cfg::getString(config), flags);
    file_.exceptions(std::ifstream::badbit);
}

std::ostream& exo::FileWriteQueue::write() {
    return file_;
}

void exo::FileWriteQueue::writeLine() {
    file_ << std::endl;
}

} // namespace exo
