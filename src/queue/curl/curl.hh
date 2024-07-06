/***
exocaster -- audio streaming helper
queue/curl/curl.hh -- HTTP GET/POST queue powered by curl

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

#ifndef QUEUE_CURL_CURL_HH
#define QUEUE_CURL_CURL_HH

#include <sstream>
#include <string>
#include <unordered_map>

#include "queue/queue.hh"
#include "refcount.hh"
#include "util.hh"

extern "C" {
#include <curl/curl.h>
}

namespace exo {

struct CURLDeleter {
    void operator()(CURL* curl) const noexcept;
};

using CURLPtr = std::unique_ptr<CURL, CURLDeleter>;

struct CurlGlobal: exo::GlobalLibrary<exo::CurlGlobal> {
    inline void init() {
        auto code = curl_global_init(CURL_GLOBAL_DEFAULT);
        if (code) throw std::runtime_error("curl_global_init failed");
    }
    inline void quit() {
        curl_global_cleanup();
    }
};

class HttpClient {
protected:
    exo::CurlGlobal global_;
    std::string url_;
    std::unordered_map<std::string, std::string> headers_;

public:
    HttpClient(const exo::ConfigObject& config,
               const std::string& instanceId);
    EXO_DEFAULT_NONCOPYABLE_DEFAULT_DESTRUCTOR(HttpClient);

    inline const auto& url() const noexcept { return url_; }
    inline const auto& headers() const noexcept { return headers_; }
};

class HttpGetReadQueue: public BaseReadQueue, private HttpClient {
    bool closed_{false};

public:
    HttpGetReadQueue(const exo::ConfigObject& config,
                     const std::string& instanceId);
    
    exo::ConfigObject readLine();
    void close();
};

class HttpPostWriteQueue: public BaseWriteQueue, private HttpClient {
    std::ostringstream buffer_;

public:
    HttpPostWriteQueue(const exo::ConfigObject& config,
                       const std::string& instanceId);
    
    std::ostream& write();
    void writeLine();
};

} // namespace exo

#endif /* QUEUE_CURL_CURL_HH */
