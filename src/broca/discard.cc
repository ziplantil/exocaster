/***
exocaster -- audio streaming helper
broca/discard.cc -- debug broca

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

#include "broca/discard.hh"
#include "broca/broca.hh"
#include "config.hh"
#include "log.hh"
#include "packet.hh"
#include "server.hh"

namespace exo {

DiscardBroca::DiscardBroca(const exo::ConfigObject& config,
                 std::shared_ptr<exo::PacketRingBuffer> source,
                 const exo::StreamFormat& streamFormat,
                 unsigned long frameRate):
                        BaseBroca(source, frameRate),
                        frameClock_(frameRate) {
    log_ = cfg::namedBoolean(config, "log", false);
    wait_ = cfg::namedBoolean(config, "wait", false);
}

void DiscardBroca::runImpl() {
    exo::byte buffer[exo::BaseBroca::DEFAULT_BROCA_BUFFER];
    while (exo::shouldRun()) {
        auto packet = source_->readPacket();
        if (!packet.has_value()) break;

        if (log_) {
            if (wait_)
                EXO_LOG("discarding %zu bytes (%zu frames, "
                                "waiting approx %4.4f seconds)",
                        packet->header.dataSize, packet->header.frameCount,
                        static_cast<double>(
                                packet->header.frameCount) / frameRate_);
            else
                EXO_LOG("discarding %zu bytes (%zu frames)",
                        packet->header.dataSize, packet->header.frameCount);
        }
        while (packet->hasData() && EXO_LIKELY(exo::shouldRun())) {
            std::size_t n = packet->readSome(buffer, sizeof(buffer));
            if (!n) {
                if (source_->closed())
                    break;
                else   
                    continue;
            }
        }

        if (wait_) {
            frameClock_.update(packet->header.frameCount);
            frameClock_.sleepIf(10);
        }
    }
}

} // namespace exo
