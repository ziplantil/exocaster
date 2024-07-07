/***
exocaster -- audio streaming helper
decoder/decoder.cc -- decoder framework

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

#include <algorithm>
#include <unordered_map>
#include <vector>

#include "decoder/decoder.hh"
#include "decoder/silence.hh"
#include "decoder/testcard.hh"

#if EXO_LIBAVCODEC
#include "decoder/libavcodec/lavc.hh"
#define DECODER_DEFS_LIBAVCODEC                                                \
    DECODER_DEF(lavc, exo::LavcDecoder)
#else
#define DECODER_DEFS_LIBAVCODEC
#endif

namespace exo {

#define DECODER_DEFS                                                           \
    DECODER_DEF(silence, exo::SilenceDecoder)                                  \
    DECODER_DEF(testcard, exo::TestcardDecoder)                                \
    DECODER_DEFS_LIBAVCODEC

enum class DecoderImpl {
#define DECODER_DEF(name, T) name,
    DECODER_DEFS
#undef DECODER_DEF
};

static std::unordered_map<std::string, exo::DecoderImpl> decoders = {
#define DECODER_DEF(N, T) { #N, exo::DecoderImpl::N },
    DECODER_DEFS
#undef DECODER_DEF
};

std::unique_ptr<exo::BaseDecoder> createDecoder(
            const std::string& type,
            const exo::ConfigObject& config,
            exo::PcmFormat pcmFormat) {
    auto it = decoders.find(type);
    
    if (it != decoders.end()) {
        switch (it->second) {
#define DECODER_DEF(N, T)   case exo::DecoderImpl::N:                          \
                                return std::make_unique<T>(config, pcmFormat);
    DECODER_DEFS
#undef DECODER_DEF
        }
    }

    throw exo::UnknownDecoderError("unknown decoder '" + type + "'");
}

void printDecoderOptions(std::ostream& stream) {
    std::vector<std::string> options;
    for (const auto& [key, _]: decoders)
        options.push_back(key);

    std::sort(options.begin(), options.end());
    for (const auto& key: options)
        std::cout << " " << key;
}

} // namespace exo
