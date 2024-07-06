/***
exocaster -- audio streaming helper
metadata.hh -- metadata types

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

#ifndef METADATA_HH
#define METADATA_HH

#include <bit>
#include <locale>
#include <string>
#include <unordered_map>

namespace exo {

struct CaseInsensitiveHash_ {
    std::size_t operator()(const std::string& s) const noexcept {
        std::size_t h = static_cast<std::size_t>(0xB3827798F1A9F17CULL);
        std::size_t p = 54907;
        for (char c: s)
            h = std::rotl((h ^ static_cast<unsigned char>(c)) * p, 3);
        return h;
    }
};

inline int caseInsensitiveCharCompare_(char a, char b) {
    return std::tolower(a, std::locale::classic())
         - std::tolower(b, std::locale::classic());
}

inline bool caseInsensitiveCharEquals_(char a, char b) {
    return caseInsensitiveCharCompare_(a, b) == 0;
}

struct CaseInsensitiveEqual_ {
    bool operator()(const std::string& s,
                    const std::string& t) const noexcept {
        return s.size() == t.size() &&
                std::equal(s.begin(), s.end(), t.begin(),
                exo::caseInsensitiveCharEquals_);
    }
};

inline int stricmp(const char* a, const char* b) {
    for (;;) {
        char x = *a++, y = *b++;
        int c = caseInsensitiveCharCompare_(x, y);
        if (c != 0 || !x) return c;
    }
    return 0;
}

inline int strnicmp(const char* a, const char* b, std::size_t n) {
    for (; n; --n) {
        char x = *a++, y = *b++;
        int c = caseInsensitiveCharCompare_(x, y);
        if (c != 0 || !x) return c;
    }
    return 0;
}

/** A map with string keys that are treated as case-insensitive. */
template <typename T>
using CaseInsensitiveMap = std::unordered_map<std::string, T,
                                              exo::CaseInsensitiveHash_,
                                              exo::CaseInsensitiveEqual_>;

using Metadata = exo::CaseInsensitiveMap<std::string>;

} // namespace exo

#endif /* METADATA_HH */
