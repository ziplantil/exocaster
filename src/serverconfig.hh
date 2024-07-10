/***
exocaster -- audio streaming helper
server_config.hh -- server configuration

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

#ifndef SERVER_CONFIG_HH
#define SERVER_CONFIG_HH

#include <cstdlib>
#include <string>
#include <unordered_map>
#include <vector>

#include "config.hh"
#include "pcmtypes.hh"

namespace exo {

struct QueueConfig {
    std::string type;
    exo::ConfigObject config;

    static QueueConfig read(const exo::ConfigObject& cfg);
};

struct DecoderConfig {
    std::string type;
    exo::ConfigObject config;

    static DecoderConfig read(const exo::ConfigObject& cfg);
};

struct CommandConfig {
    std::unordered_map<std::string, exo::DecoderConfig> commands;

    static CommandConfig read(const exo::ConfigObject& cfg);
};

struct PcmBufferConfig {
    exo::PcmSampleFormat format;
    unsigned long samplerate;
    exo::PcmChannelLayout channels;
    std::size_t size;
    bool skip;
    double skipmargin;
    double skipfactor;

    static PcmBufferConfig read(const exo::ConfigObject& cfg);

    inline exo::PcmFormat pcmFormat() const noexcept {
        return exo::PcmFormat{
            .sample = format, .rate = samplerate, .channels = channels};
    }
};

struct ResamplerConfig {
    std::string type;
    exo::ConfigObject config;

    static ResamplerConfig read(const exo::ConfigObject& cfg);
};

struct BrocaConfig {
    std::string type;
    exo::ConfigObject config;

    static BrocaConfig read(const exo::ConfigObject& cfg);
};

struct OutputConfig {
    std::string type;
    std::size_t buffer;
    exo::ConfigObject config;
    std::vector<exo::BrocaConfig> broca;

    static OutputConfig read(const exo::ConfigObject& cfg);
};

struct ServerConfig {
    exo::QueueConfig shell;
    std::vector<exo::QueueConfig> publish;
    exo::CommandConfig commands;
    exo::PcmBufferConfig pcmbuffer;
    exo::ResamplerConfig resampler;
    std::vector<exo::OutputConfig> outputs;

    static ServerConfig read(const exo::ConfigObject& cfg);
};

} // namespace exo

#endif /* SERVER_CONFIG_HH */
