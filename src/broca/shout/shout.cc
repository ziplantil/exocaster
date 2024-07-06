/***
exocaster -- audio streaming helper
broca/shout/shout.cc -- broca for playback through portaudio

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

#include "broca/shout/shout.hh"
#include "broca/broca.hh"
#include "log.hh"
#include "packet.hh"
#include "server.hh"
#include "streamformat.hh"
#include "util.hh"
#include <stdexcept>

extern "C" {
#include <shout/shout.h>
};

#define EXO_SHOUT_ALLOW_OGGFLAC 1

namespace exo {

static void shoutError_(const char* file, std::size_t lineno,
                        const char* fn, shout_t* shout) {
    exo::log(file, lineno, "%s failed: %s", fn, shout_get_error(shout));
}

#define EXO_SHOUT_ERROR(fn, shout_) \
    exo::shoutError_(__FILE__, __LINE__, fn, shout_)

exo::ShoutBroca::ShoutBroca(const exo::ConfigObject& config,
               std::shared_ptr<exo::PacketRingBuffer> source,
               const exo::StreamFormat& streamFormat,
               unsigned long frameRate): BaseBroca(source, frameRate) {
    if (!cfg::isObject(config))
        throw std::runtime_error("shout broca needs a config object");

    auto protocolString = cfg::namedString(config, "protocol");
    unsigned protocol;
    if (protocolString == "http")
        protocol = SHOUT_PROTOCOL_HTTP;
    else if (protocolString == "icy")
        protocol = SHOUT_PROTOCOL_ICY;
    else if (protocolString == "roaraudio")
        protocol = SHOUT_PROTOCOL_ROARAUDIO;
    else
        throw std::runtime_error("shout broca unsupported protocol: "
                                 "must be http/icy/roaraudio");

    auto host = cfg::namedString(config, "host");
    auto port = cfg::namedUInt<unsigned short>(config, "port");
    auto user = cfg::namedString(config, "user");
    auto password = cfg::namedString(config, "password");
    auto mount = cfg::namedString(config, "mount");

    unsigned format;
    if (!std::holds_alternative<exo::EncodedStreamFormat>(streamFormat))
        throw std::runtime_error("shout broca requires an encoded format");
    auto fmt = std::get<exo::EncodedStreamFormat>(streamFormat);
    switch (fmt.codec) {
    case exo::EncodedStreamFormatCodec::OGG_VORBIS:
    case exo::EncodedStreamFormatCodec::OGG_OPUS:
#if EXO_SHOUT_ALLOW_OGGFLAC
    case exo::EncodedStreamFormatCodec::OGG_FLAC:
#endif
        format = SHOUT_FORMAT_OGG;
        break;
    case exo::EncodedStreamFormatCodec::MP3:
        format = SHOUT_FORMAT_MP3;
        break;
    default:
        EXO_UNREACHABLE;
#if !EXO_SHOUT_ALLOW_OGGFLAC
    case exo::EncodedStreamFormatCodec::OGG_FLAC:
#endif
        throw std::runtime_error("shout broca unsupported codec");
    }
    
    shout_ = shout_new();
    if (!shout_) throw std::runtime_error("shout_new failed");

    if (shout_set_protocol(shout_, protocol) != SHOUTERR_SUCCESS) {
        EXO_SHOUT_ERROR("shout_set_protocol", shout_);
        throw std::runtime_error("shout_set_protocol failed");
    }
    if (shout_set_host(shout_, host.c_str()) != SHOUTERR_SUCCESS) {
        EXO_SHOUT_ERROR("shout_set_error", shout_);
        throw std::runtime_error("shout_set_error failed");
    }
    if (shout_set_port(shout_, port) != SHOUTERR_SUCCESS) {
        EXO_SHOUT_ERROR("shout_set_port", shout_);
        throw std::runtime_error("shout_set_port failed");
    }
    if (shout_set_user(shout_, user.c_str()) != SHOUTERR_SUCCESS) {
        EXO_SHOUT_ERROR("shout_set_user", shout_);
        throw std::runtime_error("shout_set_user failed");
    }
    if (shout_set_password(shout_, password.c_str()) != SHOUTERR_SUCCESS) {
        EXO_SHOUT_ERROR("shout_set_password", shout_);
        throw std::runtime_error("shout_set_password failed");
    }
    if (shout_set_mount(shout_, mount.c_str()) != SHOUTERR_SUCCESS) {
        EXO_SHOUT_ERROR("shout_set_mount", shout_);
        throw std::runtime_error("shout_set_mount failed");
    }
    if (shout_set_content_format(shout_, format, SHOUT_USAGE_AUDIO, nullptr)) {
        EXO_SHOUT_ERROR("shout_set_content_format", shout_);
        throw std::runtime_error("shout_set_content_format failed");
    }
}

exo::ShoutBroca::~ShoutBroca() noexcept {
    if (shout_) shout_close(shout_);
}

static bool shoutTrySend(shout_t* shout, const exo::byte* buffer,
                         std::size_t n, unsigned tries = 3) {
    int err;
    while (tries--) {
        err = shout_send(shout, buffer, n);
        switch (err) {
        case SHOUTERR_SUCCESS:
            return true;
        case SHOUTERR_RETRY:
        case SHOUTERR_SOCKET:
            continue;
        default:
            return false;
        }
    }
    return false;
}

void exo::ShoutBroca::runImpl() {
    int err;
    bool quitting = false;
    exo::byte buffer[exo::BaseBroca::DEFAULT_BROCA_BUFFER];

    while (EXO_LIKELY(exo::shouldRun(exo::QuitStatus::QUITTING))) {
        err = shout_open(shout_);
        if (err != SHOUTERR_SUCCESS) {
            EXO_SHOUT_ERROR("shout_open", shout_);
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }

        while (EXO_LIKELY(exo::shouldRun(exo::QuitStatus::QUITTING))) {
            auto packet = source_->readPacket();
            if (!packet.has_value()) {
                quitting = true;
                break;
            }

            while (packet->hasData() && EXO_LIKELY(
                        exo::shouldRun(exo::QuitStatus::QUITTING))) {
                std::size_t n = packet->readSome(buffer, sizeof(buffer));
                if (!n) {
                    if (source_->closed()) {
                        quitting = true;
                        break;
                    } else   
                        continue;
                }

                if (!exo::shoutTrySend(shout_, buffer, n)) {
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    goto reconnect;
                }
            }

            shout_sync(shout_);
        }

reconnect:
        shout_close(shout_);
        if (quitting) break;
    }
}

} // namespace exo
