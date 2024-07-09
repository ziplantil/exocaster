/***
exocaster -- audio streaming helper
encoder/ogg/ogg.hh -- Ogg slots

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

#ifndef ENCODER_OGG_OGG_HH
#define ENCODER_OGG_OGG_HH

#include <stdexcept>

#include "log.hh"
#include "slot.hh"
#include "util.hh"

extern "C" {
#include <ogg/ogg.h>
}

namespace exo {

struct OggStreamState : public ValueSlot<OggStreamState, ogg_stream_state> {
    inline OggStreamState(int serial) {
        int err = ogg_stream_init(get(), serial);
        if (err) {
            EXO_LOG("ogg_stream_init failed: %d", err);
            throw std::runtime_error("ogg_stream_init failed");
        }
    }

    inline ~OggStreamState() noexcept { ogg_stream_clear(get()); }

    EXO_DEFAULT_NONMOVABLE(OggStreamState)
};

} // namespace exo

#endif /* ENCODER_OGG_OGG_HH */
