/***
exocaster -- audio streaming helper
queue/queueutil.hh -- queue utilities

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

#ifndef QUEUE_UTIL_HH
#define QUEUE_UTIL_HH

#include <cstring>
#include <iostream>

namespace exo {

class LineInputStreamBuf_ : public std::streambuf {
    std::istream& stream_;
    char tmp_[256];
    bool eof_{false};

public:
    LineInputStreamBuf_(std::istream& stream) : stream_(stream) { }

    int underflow() {
        if (!eof_ && this->gptr() == this->egptr()) {
            std::size_t size;
            auto exceptions = stream_.exceptions();
            auto intendedExceptions = (exceptions | std::ios::badbit)
                        & ~(std::ios::failbit | std::ios::eofbit);

            if (exceptions != intendedExceptions)
                stream_.exceptions(intendedExceptions);

            stream_.getline(tmp_, sizeof(tmp_));
            if (stream_.eof())
                size = std::strlen(tmp_); // reached eof
            else if (stream_.fail()) {
                size = sizeof(tmp_) - 1;    // line still continues
                stream_.clear();
            } else {
                const char* eol = std::strchr(tmp_, '\n');
                if (!eol) {
                    size = eol - tmp_;
                    eof_ = true;
                } else
                    size = std::strlen(tmp_);
            }

            if (exceptions != intendedExceptions)
                stream_.exceptions(exceptions);
            this->setg(this->tmp_, this->tmp_, this->tmp_ + size);
        }
        return this->gptr() == this->egptr()
             ? std::char_traits<char>::eof()
             : std::char_traits<char>::to_int_type(*this->gptr());
    }
};

class LineInputStreamWithBuf_ {
protected:
    exo::LineInputStreamBuf_ buf_;
    LineInputStreamWithBuf_(std::istream& buf): buf_(buf) {}
};

class LineInputStream : virtual LineInputStreamWithBuf_,
                        public std::istream {
public:
    LineInputStream(std::istream& input)
        : LineInputStreamWithBuf_(input)
        , std::ios(&this->buf_)
        , std::istream(&this->buf_) {}
};

} // namespace exo

#endif /* QUEUE_UTIL_HH */
