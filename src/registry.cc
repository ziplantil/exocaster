/***
exocaster -- audio streaming helper
registry.cc -- server component creation

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

#include <new>
#include <stdexcept>
#include <utility>

#include "broca/broca.hh"
#include "decoder/decoder.hh"
#include "encoder/encoder.hh"
#include "packet.hh"
#include "pcmtypes.hh"
#include "registry.hh"
#include "resampler/resampler.hh"
#include "serverconfig.hh"

namespace exo {

std::shared_ptr<exo::PcmSplitter>
createPcmBuffers(const exo::PcmBufferConfig& config,
                 std::shared_ptr<exo::Publisher> publisher) {
    return std::make_shared<exo::PcmSplitter>(config.pcmFormat(), config.size,
                                              publisher);
}

void registerCommands(
    std::unordered_map<std::string, std::unique_ptr<exo::BaseDecoder>>& cmds,
    const exo::CommandConfig& config, const exo::PcmFormat& pcmFormat) {
    for (const auto& pair : config.commands) {
        cmds.insert_or_assign(pair.first, exo::createDecoder(pair.second.type,
                                                             pair.second.config,
                                                             pcmFormat));
    }
}

void registerOutputs(std::vector<std::unique_ptr<exo::BaseEncoder>>& encoders,
                     std::vector<std::unique_ptr<exo::BaseBroca>>& brocas,
                     std::vector<std::shared_ptr<exo::Barrier>>& barriers,
                     exo::PcmSplitter& pcmSplitter,
                     const std::vector<exo::OutputConfig>& configs,
                     const exo::PcmBufferConfig& bufferConfig,
                     const exo::PcmFormat& pcmFormat,
                     const exo::ResamplerConfig& resamplerConfig,
                     std::shared_ptr<exo::Publisher> publisher) {
    unsigned totalBrocas = 0;
    exo::StandardResamplerFactory resamplerFactory(
        resamplerConfig.type, resamplerConfig.config, pcmFormat);
    std::unordered_map<std::string, std::shared_ptr<exo::Barrier>>
        barriersByName;

    std::size_t brocaIndex = 0;
    for (const auto& encoderConfig : configs) {
        if (encoderConfig.broca.empty()) {
            pcmSplitter.skipIndex();
            continue;
        }

        std::shared_ptr<exo::Barrier> barrier;
        if (!encoderConfig.barrier.empty()) {
            auto it = barriersByName.find(encoderConfig.barrier);
            if (it != barriersByName.end()) {
                barrier = it->second;
            } else {
                barrier = std::make_shared<exo::Barrier>();
                barriers.push_back(barrier);
                barriersByName.insert_or_assign(encoderConfig.barrier, barrier);
            }
        }

        auto encoder =
            exo::createEncoder(encoderConfig.type, encoderConfig.config,
                               pcmSplitter.addBuffer(bufferConfig), pcmFormat,
                               resamplerFactory, barrier);
        const auto& streamFormat = encoder->streamFormat();
        auto frameRate = encoder->outputFrameRate();
        if (!frameRate)
            frameRate = pcmFormat.rate;

        for (const auto& brocaConfig : encoderConfig.broca) {
            auto encodedBuffer =
                std::make_shared<exo::PacketRingBuffer>(encoderConfig.buffer);
            if (!encodedBuffer)
                throw std::bad_alloc();
            auto broca = exo::createBroca(brocaConfig.type, brocaConfig.config,
                                          encodedBuffer, streamFormat,
                                          frameRate, publisher, brocaIndex++);
            encoder->addSink(encodedBuffer);
            brocas.push_back(std::move(broca));
            if (totalBrocas++ > exo::MAX_BROCAS)
                throw std::runtime_error("too many brocas configured");
        }

        encoders.push_back(std::move(encoder));
    }
}

std::unique_ptr<exo::BaseReadQueue>
createReadQueue(const exo::QueueConfig& queue, const std::string& instanceId) {
    return exo::createReadQueue(queue.type, queue.config, instanceId);
}

std::unique_ptr<exo::BaseWriteQueue>
createWriteQueue(const exo::QueueConfig& queue, const std::string& instanceId) {
    return exo::createWriteQueue(queue.type, queue.config, instanceId);
}

} // namespace exo
