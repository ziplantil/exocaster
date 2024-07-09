/***
exocaster -- audio streaming helper
server_config.cc -- server configuration

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

#include "serverconfig.hh"
#include "config.hh"
#include "pcmtypes.hh"
#include <stdexcept>

namespace exo {

exo::QueueConfig QueueConfig::read(const exo::ConfigObject& c) {
    auto type = cfg::mustRead<std::string>(c, "<queue>", "type");
    return exo::QueueConfig{.type = type,
                            .config = cfg::hasKey(c, "config")
                                          ? cfg::key(c, "config")
                                          : cfg::empty()};
}

static std::vector<exo::QueueConfig> readPublish(const exo::ConfigObject& c) {
    if (cfg::isNull(c))
        return {};
    if (!cfg::isArray(c))
        throw exo::InvalidConfigError("'publish' must be an array");

    std::vector<exo::QueueConfig> publish;
    for (const auto& output : cfg::iterateArray(c))
        publish.push_back(exo::QueueConfig::read(output));
    return publish;
}

exo::DecoderConfig DecoderConfig::read(const exo::ConfigObject& c) {
    auto type = cfg::mustRead<std::string>(c, "<decoder>", "type");
    return exo::DecoderConfig{.type = type,
                              .config = cfg::hasKey(c, "config")
                                            ? cfg::key(c, "config")
                                            : cfg::empty()};
}

exo::CommandConfig CommandConfig::read(const exo::ConfigObject& c) {
    std::unordered_map<std::string, exo::DecoderConfig> map;

    for (const auto& pair : cfg::iterateObject(c)) {
        if (!cfg::isObject(pair.value()))
            throw exo::InvalidConfigError("values in the 'commands' "
                                          "must be decoder configs");
        map.insert_or_assign(pair.key(),
                             exo::DecoderConfig::read(pair.value()));
    }

    return exo::CommandConfig{map};
}

static exo::PcmSampleFormat readPcmFormat(const std::string& string) {
    if (string == "u8")
        return exo::PcmSampleFormat::U8;
    else if (string == "s8")
        return exo::PcmSampleFormat::S8;
    else if (string == "s16")
        return exo::PcmSampleFormat::S16;
    else if (string == "f32")
        return exo::PcmSampleFormat::F32;
    // S24 is currently for internal use only
    // and is not accessible through config
    throw exo::InvalidConfigError("unsupported PCM format '" + string + "'");
}

static exo::PcmChannelLayout readChannelLayout(const std::string& string) {
    if (string == "mono")
        return exo::PcmChannelLayout::Mono;
    else if (string == "stereo")
        return exo::PcmChannelLayout::Stereo;
    throw exo::InvalidConfigError("unsupported channel layout '" + string +
                                  "'");
}

exo::PcmBufferConfig PcmBufferConfig::read(const exo::ConfigObject& c) {
    auto format = cfg::mayRead<std::string>(c, "pcmbuffer", "format", "s16");
    auto samplerate =
        cfg::mayRead<unsigned long>(c, "pcmbuffer", "samplerate", 44100);
    auto channelsString =
        cfg::mayRead<std::string>(c, "pcmbuffer", "channels", "stereo");
    auto duration = cfg::mayRead<double>(c, "pcmbuffer", "duration", 1.0);
    auto skip = cfg::mayRead<bool>(c, "pcmbuffer", "skip", true);
    auto skipmargin = cfg::mayRead<double>(c, "pcmbuffer", "skipmargin", 0.1);
    auto skipfactor = cfg::mayRead<double>(c, "pcmbuffer", "skipfactor", 2.0);

    if (duration < 0)
        throw std::runtime_error("duration cannot be negative");
    if (skipmargin < 0)
        throw std::runtime_error("skipmargin cannot be negative");
    if (skipfactor < 0)
        throw std::runtime_error("skipfactor cannot be negative");

    auto pcmFormat = exo::readPcmFormat(format);
    auto channels = exo::readChannelLayout(channelsString);

    return exo::PcmBufferConfig{
        .format = pcmFormat,
        .samplerate = samplerate,
        .channels = channels,
        .size = static_cast<std::size_t>(duration * samplerate *
                                         exo::channelCount(channels) *
                                         exo::bytesPerSampleFormat(pcmFormat)),
        .skip = skip,
        .skipmargin = skipmargin,
        .skipfactor = skipfactor,
    };
}

exo::ResamplerConfig ResamplerConfig::read(const exo::ConfigObject& c) {
    if (cfg::isNull(c))
        return {};

    auto type = cfg::mayRead<std::string>(c, "resampler", "type", "");

    return exo::ResamplerConfig{.type = type,
                                .config = cfg::hasKey(c, "config")
                                              ? cfg::key(c, "config")
                                              : cfg::empty()};
}

exo::BrocaConfig BrocaConfig::read(const exo::ConfigObject& c) {
    auto type = cfg::mustRead<std::string>(c, "<broca>", "type");
    return exo::BrocaConfig{.type = type,
                            .config = cfg::hasKey(c, "config")
                                          ? cfg::key(c, "config")
                                          : cfg::empty()};
}

static std::vector<exo::BrocaConfig> readBrocas(const exo::ConfigObject& c) {
    if (!cfg::isArray(c))
        throw exo::InvalidConfigError("'broca' must be an array");

    std::vector<exo::BrocaConfig> brocas;
    for (const auto& broca : cfg::iterateArray(c))
        brocas.push_back(exo::BrocaConfig::read(broca));
    return brocas;
}

exo::OutputConfig OutputConfig::read(const exo::ConfigObject& c) {
    auto type = cfg::mustRead<std::string>(c, "<output>", "type");
    auto buffer = cfg::mayRead<std::size_t>(c, "<output>", "buffer", 65536);

    if (!cfg::hasKey(c, "broca"))
        throw exo::InvalidConfigError("no 'broca' field in an output config");

    return exo::OutputConfig{.type = type,
                             .buffer = buffer,
                             .config = cfg::hasKey(c, "config")
                                           ? cfg::key(c, "config")
                                           : cfg::empty(),
                             .broca = readBrocas(cfg::key(c, "broca"))};
}

static std::vector<exo::OutputConfig> readOutputs(const exo::ConfigObject& c) {
    if (!cfg::isArray(c))
        throw exo::InvalidConfigError("'outputs' must be an array");

    std::vector<exo::OutputConfig> outputs;
    for (const auto& output : cfg::iterateArray(c))
        outputs.push_back(exo::OutputConfig::read(output));
    return outputs;
}

exo::ServerConfig ServerConfig::read(const exo::ConfigObject& c) {
    if (!cfg::hasObject(c, "shell"))
        throw exo::InvalidConfigError("no 'shell' field in config");
    if (!cfg::hasObject(c, "commands"))
        throw exo::InvalidConfigError("no 'commands' field in config");
    if (!cfg::hasArray(c, "outputs"))
        throw exo::InvalidConfigError("no 'outputs' field in config");

    return exo::ServerConfig{
        .shell = exo::QueueConfig::read(cfg::key(c, "shell")),
        .publish = cfg::hasArray(c, "publish")
                       ? exo::readPublish(cfg::key(c, "publish"))
                       : exo::readPublish(cfg::empty()),
        .commands = exo::CommandConfig::read(cfg::key(c, "commands")),
        .pcmbuffer = cfg::hasObject(c, "pcmbuffer")
                         ? exo::PcmBufferConfig::read(cfg::key(c, "pcmbuffer"))
                         : exo::PcmBufferConfig::read(cfg::empty()),
        .resampler = cfg::hasObject(c, "resampler")
                         ? exo::ResamplerConfig::read(cfg::key(c, "resampler"))
                         : exo::ResamplerConfig::read(cfg::empty()),
        .outputs = exo::readOutputs(cfg::key(c, "outputs")),
    };
}

} // namespace exo
