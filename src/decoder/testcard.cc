/***
exocaster -- audio streaming helper
decoder/testcard.cc -- test card tone generator

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

#include <cmath>
#include <numbers>

#include "config.hh"
#include "decoder/testcard.hh"
#include "log.hh"
#include "pcmconvert.hh"
#include "pcmtypes.hh"
#include "server.hh"

namespace exo {

void TestcardDecodeJob::run(std::shared_ptr<exo::PcmSplitter> sink) {
    exo::byte block[8192];
    std::size_t framesPerBlock = sizeof(block) / pcmFormat_.bytesPerFrame();

    constexpr double tau = 2 * std::numbers::pi_v<double>;
    double f = tau * 1000.0 / pcmFormat_.rate;
    double x = 0.0;

    sink->metadata(command_, {});
    while (EXO_LIKELY(exo::shouldRun() && frames_ > 0)) {
        std::size_t framesThisBlock = std::min(frames_, framesPerBlock);
        exo::byte* destination = block;

        for (std::size_t i = 0; i < framesThisBlock; ++i) {
            double v = std::sin(x + f * i) * 0.75;
            for (unsigned j = 0;
                        j < exo::channelCount(pcmFormat_.channels); ++j)
                destination = exo::outputSample(destination,
                                                pcmFormat_.sample, v);
        }
        x = std::fmod(x + f * framesThisBlock, tau);

        frames_ -= framesThisBlock;
        sink->pcm({ block, static_cast<std::size_t>(destination - block) });
    }
}

std::optional<std::unique_ptr<BaseDecodeJob>> TestcardDecoder::createJob(
            const exo::ConfigObject& request,
            std::shared_ptr<exo::ConfigObject> command) {
    if (!cfg::isFloat(request)) {
        EXO_LOG("testcard decoder: "
                "config not a non-negative number, ignoring.");
        return {};
    }

    auto duration = cfg::getFloat(request);
    if (duration <= 0) {
        EXO_LOG("testcard decoder: "
                "config not a non-negative number, ignoring.");
        return {};
    }

    auto frames = pcmFormat_.durationToFrameCount(duration);
    return { std::make_unique<exo::TestcardDecodeJob>(sink_, pcmFormat_,
                command, frames) };
}

} // namespace exo
