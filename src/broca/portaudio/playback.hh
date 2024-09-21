/***
exocaster -- audio streaming helper
broca/portaudio/playback.hh -- broca for playback through portaudio

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

#ifndef BROCA_PORTAUDIO_PLAYBACK_HH
#define BROCA_PORTAUDIO_PLAYBACK_HH

#include <cstddef>
#include <memory>

#include "broca/broca.hh"
#include "config.hh"
#include "packet.hh"
#include "refcount.hh"
#include "slot.hh"
#include "streamformat.hh"
#include "util.hh"

extern "C" {
#include <portaudio.h>
}

namespace exo {

struct PortAudioGlobal : exo::GlobalLibrary<exo::PortAudioGlobal> {
    void init();
    void quit();
};

struct PortAudioStream : public PointerSlot<PortAudioStream, PaStream> {
    using PointerSlot::PointerSlot;
    PortAudioStream(PaStream* p);
    ~PortAudioStream() noexcept;
    EXO_DEFAULT_NONCOPYABLE(PortAudioStream)
};

class PortAudioBroca : public exo::BaseBroca {
    exo::PortAudioGlobal global_;
    PortAudioStream stream_;
    std::size_t bytesPerFrame_;
    PacketRingBuffer::PacketRead packet_;

  protected:
    void runImpl();

  public:
    PortAudioBroca(const exo::ConfigObject& config,
                   std::shared_ptr<exo::PacketRingBuffer> source,
                   const exo::StreamFormat& streamFormat,
                   unsigned long frameRate,
                   const std::shared_ptr<exo::Publisher>& publisher,
                   std::size_t brocaIndex);

    int streamCallback(const void* input, void* output,
                       unsigned long frameCount,
                       const PaStreamCallbackTimeInfo* timeInfo,
                       PaStreamCallbackFlags statusFlags);
};

} // namespace exo

#endif /* BROCA_PORTAUDIO_PLAYBACK_HH */
