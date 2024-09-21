/***
exocaster -- audio streaming helper
packetstream.hh -- packet stream

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

#ifndef QUEUE_PACKETSTREAM_HH
#define QUEUE_PACKETSTREAM_HH

#include <iostream>

#include "packet.hh"

namespace exo {

class PacketInputStreamBuf_ : public std::streambuf {
    exo::PacketRingBuffer::PacketRead& packet_;
    char tmp_[512];
    bool eof_{false};

  public:
    PacketInputStreamBuf_(exo::PacketRingBuffer::PacketRead& packet)
        : packet_(packet) {}

    int underflow() {
        if (!eof_ && this->gptr() == this->egptr()) {
            std::size_t n = packet_.readSome(tmp_, sizeof(tmp_));
            this->setg(this->tmp_, this->tmp_, this->tmp_ + n);
        }
        return this->gptr() == this->egptr()
                   ? std::char_traits<char>::eof()
                   : std::char_traits<char>::to_int_type(*this->gptr());
    }
};

class PacketInputStreamWithBuf_ {
  protected:
    exo::PacketInputStreamBuf_ buf_;
    PacketInputStreamWithBuf_(exo::PacketRingBuffer::PacketRead& packet)
        : buf_(packet) {}
};

class PacketInputStream : virtual PacketInputStreamWithBuf_,
                          public std::istream {
  public:
    PacketInputStream(exo::PacketRingBuffer::PacketRead& packet)
        : PacketInputStreamWithBuf_(packet), std::ios(&this->buf_),
          std::istream(&this->buf_) {}
};

} // namespace exo

#endif /* QUEUE_PACKETSTREAM_HH */
