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

#include <chrono>
#include <cstddef>
#include <exception>
#include <new>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <variant>

#include "broca/broca.hh"
#include "broca/shout/shout.hh"
#include "config.hh"
#include "log.hh"
#include "metadata.hh"
#include "packet.hh"
#include "server.hh"
#include "streamformat.hh"
#include "types.hh"
#include "util.hh"

extern "C" {
#include <shout/shout.h>
};

namespace exo {

void ShoutGlobal::init() { shout_init(); }
void ShoutGlobal::quit() { shout_shutdown(); }

Shout::Shout() : PointerSlot(shout_new()) {
    if (!has())
        throw std::bad_alloc();
}

Shout::~Shout() noexcept {
    if (has())
        shout_free(release());
}

static void shoutError_(const char* file, std::size_t lineno, const char* fn,
                        shout_t* shout) {
    exo::log(file, lineno, "%s failed: %s", fn, shout_get_error(shout));
}

#define EXO_SHOUT_ERROR(fn, shout_)                                            \
    exo::shoutError_(__FILE__, __LINE__, fn, shout_)

static void shoutCopyMetadata(shout_t* shout, const char* shoutMeta,
                              const exo::ConfigObject& config,
                              const char* key) {
    if (cfg::hasString(config, key)) {
        auto str = cfg::namedString(config, key);
        if (shout_set_meta(shout, shoutMeta, str.c_str()) != SHOUTERR_SUCCESS) {
            EXO_SHOUT_ERROR("shout_set_meta", shout);
            throw std::runtime_error("shout_set_meta failed");
        }
    }
}

exo::ShoutBroca::ShoutBroca(const exo::ConfigObject& config,
                            std::shared_ptr<exo::PacketRingBuffer> source,
                            const exo::StreamFormat& streamFormat,
                            unsigned long frameRate,
                            const std::shared_ptr<exo::Publisher>& publisher,
                            std::size_t brocaIndex)
    : BaseBroca(source, frameRate, publisher, brocaIndex),
      syncClock_(frameRate) {
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

    auto shout = shout_.get();

    if (shout_set_protocol(shout, protocol) != SHOUTERR_SUCCESS) {
        EXO_SHOUT_ERROR("shout_set_protocol", shout);
        throw std::runtime_error("shout_set_protocol failed");
    }
    if (shout_set_host(shout, host.c_str()) != SHOUTERR_SUCCESS) {
        EXO_SHOUT_ERROR("shout_set_error", shout);
        throw std::runtime_error("shout_set_error failed");
    }
    if (shout_set_port(shout, port) != SHOUTERR_SUCCESS) {
        EXO_SHOUT_ERROR("shout_set_port", shout);
        throw std::runtime_error("shout_set_port failed");
    }
    if (shout_set_user(shout, user.c_str()) != SHOUTERR_SUCCESS) {
        EXO_SHOUT_ERROR("shout_set_user", shout);
        throw std::runtime_error("shout_set_user failed");
    }
    if (shout_set_password(shout, password.c_str()) != SHOUTERR_SUCCESS) {
        EXO_SHOUT_ERROR("shout_set_password", shout);
        throw std::runtime_error("shout_set_password failed");
    }
    if (shout_set_mount(shout, mount.c_str()) != SHOUTERR_SUCCESS) {
        EXO_SHOUT_ERROR("shout_set_mount", shout);
        throw std::runtime_error("shout_set_mount failed");
    }
    if (shout_set_content_format(shout, format, SHOUT_USAGE_AUDIO, nullptr)) {
        EXO_SHOUT_ERROR("shout_set_content_format", shout);
        throw std::runtime_error("shout_set_content_format failed");
    }

    shoutCopyMetadata(shout, SHOUT_META_NAME, config, "name");
    shoutCopyMetadata(shout, SHOUT_META_GENRE, config, "genre");
    shoutCopyMetadata(shout, SHOUT_META_DESCRIPTION, config, "description");
    shoutCopyMetadata(shout, SHOUT_META_URL, config, "url");

    double waitThresh = cfg::namedFloat(config, "selfsyncthreshold", 0.1);
    selfSync_ = cfg::namedBoolean(config, "selfsync", false);
    syncThreshold_ = static_cast<std::size_t>(waitThresh * frameRate);
}

static bool shoutTrySend(shout_t* shout, const exo::byte* buffer, std::size_t n,
                         unsigned tries = 3) {
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
            goto fail;
        }
    }
fail:
    EXO_SHOUT_ERROR("shout_send", shout);
    return false;
}

struct ShoutMetadata : public PointerSlot<ShoutMetadata, shout_metadata_t> {
    using PointerSlot::PointerSlot;
    ShoutMetadata() : PointerSlot(shout_metadata_new()) {
        if (!has())
            throw std::bad_alloc();
    }
    ~ShoutMetadata() noexcept {
        if (has())
            shout_metadata_free(release());
    }
    EXO_DEFAULT_NONCOPYABLE(ShoutMetadata)
};

void exo::ShoutBroca::handleOutOfBandMetadata_(
    exo::PacketRingBuffer::PacketRead& packet) {
    try {
        auto metadata = exo::readPacketMetadata(packet);

        // TODO eventually this should be customizable
        std::ostringstream joiner;
        const char* artist = "";
        const char* title = "";

        for (const auto& [key, value] : metadata) {
            if (!exo::stricmp(key.c_str(), "artist")) {
                artist = value.c_str();
            } else if (!exo::stricmp(key.c_str(), "title")) {
                title = value.c_str();
            }
        }

        int err;
        ShoutMetadata meta_;
        auto shout = shout_.get();
        auto meta = meta_.get();
        joiner << artist << " - " << title;
        auto joined = joiner.str();

        err = shout_metadata_add(meta, "song", joined.c_str());
        if (err != SHOUTERR_SUCCESS) {
            EXO_SHOUT_ERROR("shout_set_metadata", shout);
            return;
        }
#if 1
        err = shout_set_metadata_utf8(shout, meta);
#else
        err = shout_set_metadata(shout, meta);
#endif
        if (err != SHOUTERR_SUCCESS) {
            EXO_SHOUT_ERROR("shout_set_metadata", shout);
            return;
        }
    } catch (const std::exception& e) {
        EXO_LOG("set metadata error: %s", e.what());
    }
}

void exo::ShoutBroca::runImpl() {
    int err;
    bool quitting = false;
    exo::byte buffer[exo::BaseBroca::DEFAULT_BROCA_BUFFER];
    auto shout = shout_.get();
    long openTime = 1;

    while (EXO_LIKELY(exo::shouldRun())) {
        err = shout_open(shout);
        if (err != SHOUTERR_SUCCESS) {
            EXO_SHOUT_ERROR("shout_open", shout);
            std::this_thread::sleep_for(std::chrono::seconds(openTime));
            openTime = std::min(openTime * 2, 60L);
            continue;
        }
        openTime = 1;

        if (selfSync_)
            syncClock_.reset();

        while (EXO_LIKELY(exo::shouldRun())) {
            auto packet = source_->readPacket();
            if (!packet.has_value()) {
                quitting = true;
                break;
            }

            if (packet->header.flags & PacketFlags::MetadataPacket) {
                handleOutOfBandMetadata_(*packet);
                continue;
            }
            if (packet->header.flags & PacketFlags::OriginalCommandPacket) {
                acknowledgeCommand_(*packet);
                continue;
            }

            while (packet->hasData() && EXO_LIKELY(exo::shouldRun())) {
                std::size_t n = packet->readSome(buffer, sizeof(buffer));
                if (!n) {
                    if (source_->closed()) {
                        quitting = true;
                        break;
                    } else
                        continue;
                }

                if (!exo::shoutTrySend(shout, buffer, n)) {
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    goto reconnect;
                }
            }

            if (selfSync_) {
                syncClock_.update(packet->header.frameCount);
                syncClock_.sleepIf(syncThreshold_);
            } else {
                shout_sync(shout);
            }
        }

    reconnect:
        shout_close(shout);
        if (quitting)
            break;
    }
}

} // namespace exo
