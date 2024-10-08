/***
exocaster -- audio streaming helper
broca/broca.hh -- broca (broadcaster) framework

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

#ifndef BROCA_BROCA_HH
#define BROCA_BROCA_HH

#include <cstddef>
#include <iostream>
#include <memory>
#include <semaphore>
#include <stdexcept>
#include <string>

#include "config.hh"
#include "metadata.hh"
#include "packet.hh"
#include "publisher.hh"
#include "streamformat.hh"
#include "util.hh"

namespace exo {

class UnknownBrocaError : public std::logic_error {
  public:
    using std::logic_error::logic_error;
};

static constexpr unsigned MAX_BROCAS = 32767;

extern std::counting_semaphore<exo::MAX_BROCAS> brocaCounter;

class BaseBroca {
  protected:
    std::shared_ptr<exo::PacketRingBuffer> source_;
    unsigned long frameRate_;
    std::shared_ptr<exo::Publisher> publisher_;
    std::size_t brocaIndex_;

    virtual void runImpl() = 0;

    void acknowledgeCommand_(exo::PacketRingBuffer::PacketRead& packet) {
        if (publisher_)
            publisher_->acknowledgeBrocaCommand(brocaIndex_,
                                                exo::readPacketCommand(packet));
    }

  public:
    static constexpr std::size_t DEFAULT_BROCA_BUFFER = 4096;

    /*
    BaseBroca(const exo::ConfigObject& config,
              std::shared_ptr<exo::PacketRingBuffer> source,
              const exo::StreamFormat& streamFormat,
              unsigned long frameRate,
              const std::shared_ptr<exo::Publisher>& publisher,
              std::size_t brocaIndex);
    */
    inline BaseBroca(std::shared_ptr<exo::PacketRingBuffer> source,
                     unsigned long frameRate,
                     const std::shared_ptr<exo::Publisher>& publisher,
                     std::size_t brocaIndex)
        : source_(source), frameRate_(frameRate), publisher_(publisher),
          brocaIndex_(brocaIndex) {}
    EXO_DEFAULT_NONCOPYABLE_VIRTUAL_DESTRUCTOR(BaseBroca)

    void run();
};

std::unique_ptr<exo::BaseBroca>
createBroca(const std::string& type, const exo::ConfigObject& config,
            std::shared_ptr<exo::PacketRingBuffer> source,
            const exo::StreamFormat& streamFormat, std::size_t frameRate,
            const std::shared_ptr<exo::Publisher>& publisher,
            std::size_t brocaIndex);

void printBrocaOptions(std::ostream& stream);

} // namespace exo

#endif /* BROCA_BROCA_HH */
