/***
exocaster -- audio streaming helper
decoder/testcard.hh -- test card tone generator

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

#ifndef DECODER_TESTCARD_HH
#define DECODER_TESTCARD_HH

#include <cstddef>
#include <memory>
#include <optional>

#include "config.hh"
#include "decoder/decoder.hh"

namespace exo {

struct TestcardParameters {
    double amplitude;
    double frequency;
};

class TestcardDecodeJob : public exo::BaseDecodeJob {
  protected:
    std::size_t frames_;
    double freq_, ampl_;

  public:
    TestcardDecodeJob(std::shared_ptr<exo::PcmSplitter> sink,
                      exo::PcmFormat pcmFormat,
                      std::shared_ptr<exo::ConfigObject> command,
                      std::size_t frames,
                      const exo::TestcardParameters& params);

    void run(std::shared_ptr<exo::PcmSplitter> sink);
};

class TestcardDecoder : public exo::BaseDecoder {
    TestcardParameters params_;

  public:
    TestcardDecoder(const exo::ConfigObject& config, exo::PcmFormat pcmFormat);

    std::optional<std::unique_ptr<BaseDecodeJob>>
    createJob(const exo::ConfigObject& request,
              std::shared_ptr<exo::ConfigObject> command);
};

} // namespace exo

#endif /* DECODER_TESTCARD_HH */
