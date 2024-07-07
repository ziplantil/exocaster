/***
exocaster -- audio streaming helper
encoder/pcm.cc -- PCM passthrough encoder

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

#include "encoder/pcm.hh"
#include "log.hh"

#define EXO_DUMP_METADATA 1

namespace exo {

PcmEncoder::PcmEncoder(const exo::ConfigObject& config,
                       std::shared_ptr<exo::PcmBuffer> source,
                       exo::PcmFormat pcmFormat):
            BaseEncoder(source, pcmFormat) {
    metadata_ = cfg::namedBoolean(config, "metadata", false);
}
        
exo::StreamFormat PcmEncoder::streamFormat() const noexcept {
    return pcmFormat_;
}

void PcmEncoder::startTrack(const exo::Metadata& metadata) {
    if (metadata_) {
        EXO_LOG("pcm metadata dump");
        for (const auto& [key, value]: metadata) {
            EXO_LOG("pcm metadata : %s=%s", key.c_str(), value.c_str());
        }
    }
}

void PcmEncoder::pcmBlock(std::size_t frameCount,
                          std::span<const exo::byte> data) {
    packet(frameCount, data);
}

} // namespace exo
