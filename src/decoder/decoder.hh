/***
exocaster -- audio streaming helper
decoder/decoder.hh -- decoder framework

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

#ifndef DECODER_DECODER_HH
#define DECODER_DECODER_HH

#include <iostream>
#include <memory>
#include <optional>

#include "config.hh"
#include "jobqueue.hh"
#include "pcmbuffer.hh"
#include "pcmtypes.hh"

namespace exo {

class UnknownDecoderError : public std::logic_error {
public:
    using std::logic_error::logic_error;
};

class BaseDecodeJob: public exo::Job<std::shared_ptr<exo::PcmSplitter>> {
private:
    std::shared_ptr<exo::PcmSplitter> sink_;

protected:
    exo::PcmFormat pcmFormat_;
    std::shared_ptr<exo::ConfigObject> command_;

    inline BaseDecodeJob(std::shared_ptr<exo::PcmSplitter> sink,
                         exo::PcmFormat pcmFormat,
                         std::shared_ptr<exo::ConfigObject> command)
            : sink_(sink), pcmFormat_(pcmFormat), command_(command) { }

public:
    EXO_DEFAULT_NONCOPYABLE_VIRTUAL_DESTRUCTOR(BaseDecodeJob)

    virtual void init() { }
    virtual void run(std::shared_ptr<exo::PcmSplitter> sink) = 0;
};

class BaseDecoder {
protected:
    std::shared_ptr<exo::PcmSplitter> sink_;
    exo::PcmFormat pcmFormat_;

public:
    /*
    BaseDecoder(const exo::ConfigObject& config,
                exo::PcmFormat pcmFormat,
                std::shared_ptr<exo::ConfigObject> command);
    */
    inline BaseDecoder(exo::PcmFormat pcmFormat) : pcmFormat_(pcmFormat) { }
    EXO_DEFAULT_NONCOPYABLE_VIRTUAL_DESTRUCTOR(BaseDecoder)

    virtual std::optional<std::unique_ptr<BaseDecodeJob>> createJob(
            const exo::ConfigObject& request,
            std::shared_ptr<exo::ConfigObject> command) = 0;
};

std::unique_ptr<exo::BaseDecoder> createDecoder(
        const std::string& name,
        const exo::ConfigObject& config,
        exo::PcmFormat pcmFormat);

void printDecoderOptions(std::ostream& stream);

} // namespace exo

#endif /* DECODER_DECODER_HH */
