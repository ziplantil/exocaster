/***
exocaster -- audio streaming helper
registry.hh -- server component creation

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

#ifndef REGISTRY_HH
#define REGISTRY_HH

#include "broca/broca.hh"
#include "decoder/decoder.hh"
#include "encoder/encoder.hh"
#include "pcmbuffer.hh"
#include "queue/queue.hh"
#include "serverconfig.hh"

namespace exo {

std::shared_ptr<exo::PcmSplitter> createPcmBuffers(
                const exo::PcmBufferConfig& config,
                std::shared_ptr<exo::Publisher> publisher);

void registerCommands(
            std::unordered_map<std::string,
                    std::unique_ptr<exo::BaseDecoder>>& cmds,
            const exo::CommandConfig& config,
            const exo::PcmFormat& pcmFormat);

void registerOutputs(
            std::vector<std::unique_ptr<exo::BaseEncoder>>& encoders,
            std::vector<std::unique_ptr<exo::BaseBroca>>& brocas,
            exo::PcmSplitter& pcmSplitter,
            const std::vector<exo::OutputConfig>& configs,
            const exo::PcmBufferConfig& bufferConfig,
            const exo::PcmFormat& pcmFormat);

std::unique_ptr<exo::BaseReadQueue> createReadQueue(
                const exo::QueueConfig& queue,
                const std::string& instanceId);
std::unique_ptr<exo::BaseWriteQueue> createWriteQueue(
                const exo::QueueConfig& queue,
                const std::string& instanceId);

} // namespace exo

#endif /* REGISTRY_HH */
