/***
exocaster -- audio streaming helper
broca/file.cc -- file output broca

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

#include "broca/file.hh"
#include "config.hh"
#include "log.hh"
#include "server.hh"

namespace exo {

FileBroca::FileBroca(const exo::ConfigObject& config,
                 std::shared_ptr<exo::PacketRingBuffer> source,
                 const exo::StreamFormat& streamFormat,
                 unsigned long frameRate): BaseBroca(source, frameRate) {
    if (!cfg::isString(config) && !cfg::isObject(config))
        throw exo::InvalidConfigError("'file' broca needs a string or "
                                      "an object as config");

    std::string path;
    auto flags = std::ios::out | std::ios::binary;

    if (cfg::isString(config)) {
        path = cfg::getString(config);
    } else {
        if (!cfg::hasString(config, "file")) {
            throw exo::InvalidConfigError("'file' broca config needs 'file'");
        }
        path = cfg::namedString(config, "file");
        if (cfg::namedBoolean(config, "append", false))
            flags |= std::ios::app;
    }

    file_.exceptions(std::ios::failbit | std::ios::badbit);
    file_.open(path, flags);
    file_.exceptions();
}

void FileBroca::runImpl() {
    exo::byte buffer[exo::BaseBroca::DEFAULT_BROCA_BUFFER];
    while (exo::shouldRun(exo::QuitStatus::QUITTING)) {
        auto packet = source_->readPacket();
        if (!packet.has_value()) break;

        while (packet->hasData() && EXO_LIKELY(
                    exo::shouldRun(exo::QuitStatus::QUITTING))) {
            std::size_t n = packet->readSome(buffer, sizeof(buffer));
            if (!n) {
                if (source_->closed())
                    break;
                else   
                    continue;
            }
            
            file_.write(reinterpret_cast<const char*>(buffer), n);
            if (file_.bad()) {
                auto error = std::system_error(errno,
                            std::system_category());
                EXO_LOG("failed to write to file: %s", error.what());
                return;
            }
        }
    }
    file_.flush();
}

} // namespace exo
