/***
exocaster -- audio streaming helper
broca/shout/shout.hh -- broca for playback through portaudio

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

#ifndef BROCA_SHOUT_SHOUT_HH
#define BROCA_SHOUT_SHOUT_HH

#include <memory>

#include "broca/broca.hh"
#include "config.hh"
#include "fclock.hh"
#include "packet.hh"
#include "refcount.hh"
#include "slot.hh"
#include "streamformat.hh"
#include "util.hh"

extern "C" {
#include <shout/shout.h>
};

namespace exo {

struct ShoutGlobal : exo::GlobalLibrary<exo::ShoutGlobal> {
    void init();
    void quit();
};

struct Shout : public PointerSlot<Shout, shout_t> {
    using PointerSlot::PointerSlot;
    Shout();
    ~Shout() noexcept;
    EXO_DEFAULT_NONCOPYABLE(Shout)
};

class ShoutBroca : public exo::BaseBroca {
  private:
    exo::ShoutGlobal global_;
    Shout shout_;
    bool selfSync_;
    exo::FrameClock<> syncClock_;
    unsigned long syncThreshold_;

    void handleOutOfBandMetadata_(exo::PacketRingBuffer::PacketRead& packet);

  protected:
    void runImpl();

  public:
    ShoutBroca(const exo::ConfigObject& config,
               std::shared_ptr<exo::PacketRingBuffer> source,
               const exo::StreamFormat& streamFormat, unsigned long frameRate);
};

} // namespace exo

#endif /* BROCA_SHOUT_SHOUT_HH */
