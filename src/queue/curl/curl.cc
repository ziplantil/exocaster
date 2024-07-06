/***
exocaster -- audio streaming helper
queue/curl/curl.cc -- HTTP GET/POST queue powered by curl

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

#include <stdexcept>
#include <thread>

#include "queue/curl/curl.hh"
#include "log.hh"
#include "server.hh"
#include "util.hh"
#include "version.hh"

extern "C" {
#include <curl/curl.h>
}

namespace exo {

exo::HttpClient::HttpClient(const exo::ConfigObject& config,
                            const std::string& instanceId) {
    if (!cfg::isObject(config))
        throw std::runtime_error("http client config must be an object");
    if (!cfg::hasString(config, "url"))
        throw std::runtime_error("http client config must have 'url'");

    url_ = cfg::namedString(config, "url");

    if (cfg::hasObject(config, "headers")) {
        auto headers = cfg::key(config, "headers");
        for (const auto& [key, value]: cfg::iterateObject(headers))
            headers_.insert_or_assign(key, value);
    }
    
    if (cfg::hasString(config, "instanceParameter")) {
        CURLU* url = curl_url();
        if (!url) throw std::bad_alloc();

        auto queryParameter = cfg::namedString(config, "instanceParameter")
                                    + "=" + instanceId;

        int ret;
        ret = curl_url_set(url, CURLUPART_URL, url_.c_str(), 0);
        if (!ret)
            ret = curl_url_set(url, CURLUPART_QUERY,
                               queryParameter.c_str(), CURLU_APPENDQUERY);
        char* formatted;
        if (!ret)
            ret = curl_url_get(url, CURLUPART_URL, &formatted, 0);
        if (ret)
            throw std::bad_alloc();

        url_ = std::string(formatted);
        curl_free(formatted);
        curl_url_cleanup(url);
    }
}

void exo::CURLDeleter::operator()(CURL* curl) const noexcept {
    if (curl) curl_easy_cleanup(curl);
}

static void curlError_(const char* file, std::size_t lineno,
                       const char* fn, CURLcode ret) {
    exo::log(file, lineno, "%s failed (%d): %s", fn, ret,
                curl_easy_strerror(ret));
}

#define EXO_CURL_ERROR(fn, ret) exo::curlError_(__FILE__, __LINE__, fn, ret)

static exo::CURLPtr getCurl() {
    CURL* curl{nullptr};
    do {
        curl = curl_easy_init();
        if (!curl) {
            EXO_LOG("curl_easy_init failed - trying again");
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    } while (!curl);
    return exo::CURLPtr(curl);
};

exo::HttpGetReadQueue::HttpGetReadQueue(const exo::ConfigObject& config,
                                        const std::string& instanceId):
                HttpClient(config, instanceId) { }

template <typename T, std::size_t N>
constexpr int arraySize(T(&)[N]) {
    return N;
}

static const char* userAgent = "exocaster/" EXO_VERSION;

static const char* staticHeadersGet[] = {
    "Accept: application/json"
};

static const char* staticHeadersPost[] = {
    "Content-Type: application/json"
};

struct CURLSListDeleter {
    void operator()(struct curl_slist* list) const noexcept {
        if (list) curl_slist_free_all(list);
    }
};

using CURLSListPtr = std::unique_ptr<struct curl_slist, CURLSListDeleter>;

static bool copyUrlHeaders(const HttpClient& client,
                           CURLPtr& curlPtr, CURLSListPtr& listOwner,
                           const char* staticHeaders[],
                           std::size_t staticHeadersCount) {
    auto curl = curlPtr.get();

    struct curl_slist* hlist = nullptr;
    struct curl_slist* nlist = nullptr;

    for (const auto& [key, value]: client.headers()) {
        nlist = curl_slist_append(hlist, (key + ": " + value).c_str());
        if (!nlist) {
            if (hlist) curl_slist_free_all(hlist);
            EXO_LOG("curl_slist_append failed");
            return false;
        }
        hlist = nlist;
    }

    for (std::size_t i = 0; i < staticHeadersCount; ++i) {
        nlist = curl_slist_append(hlist, staticHeaders[i]);
        if (!nlist) {
            if (hlist) curl_slist_free_all(hlist);
            EXO_LOG("curl_slist_append failed");
            return false;
        }
        hlist = nlist;
    }

    listOwner.reset(hlist);

    CURLcode ret;
    if ((ret = curl_easy_setopt(curl, CURLOPT_URL, client.url().c_str()))
                != CURLE_OK) {
        EXO_CURL_ERROR("curl_easy_setopt(CURLOPT_URL)", ret);
        return false;
    }
    if ((ret = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hlist))
                != CURLE_OK) {
        EXO_CURL_ERROR("curl_easy_setopt(CURLOPT_HTTPHEADER)", ret);
        return false;
    }
    if ((ret = curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1))
                != CURLE_OK) {
        EXO_CURL_ERROR("curl_easy_setopt(CURLOPT_FOLLOWLOCATION)", ret);
        return false;
    }
    if ((ret = curl_easy_setopt(curl, CURLOPT_USERAGENT, userAgent))
                != CURLE_OK) {
        EXO_CURL_ERROR("curl_easy_setopt(CURLOPT_USERAGENT)", ret);
        return false;
    }
    return true;
}

static std::size_t toStringStream_(const void* ptr, std::size_t size,
                                   std::size_t count, void* pStream) {
    reinterpret_cast<std::ostringstream*>(pStream)->
            write(static_cast<const char*>(ptr), size * count);
    return count;    
}

exo::ConfigObject exo::HttpGetReadQueue::readLine() {
    bool first = true;

    for (;;) {
        CURLSListPtr slistPtr;
        CURLPtr curlPtr = exo::getCurl();
        auto curl = curlPtr.get();

        if (first)
            first = false;
        else if (EXO_UNLIKELY(closed_ ||
                    !exo::shouldRun(exo::QuitStatus::NO_MORE_COMMANDS)))
            break;
        else
            std::this_thread::sleep_for(std::chrono::seconds(1));

        bool ok = exo::copyUrlHeaders(*this, curlPtr, slistPtr,
                                      staticHeadersGet,
                                      exo::arraySize(staticHeadersGet));
        if (!ok) continue;
    
        CURLcode ret;
        std::ostringstream stream;
        if ((ret = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, 
                    &exo::toStringStream_)) != CURLE_OK) {
            EXO_CURL_ERROR("curl_easy_setopt(CURLOPT_WRITEFUNCTION)", ret);
            continue;
        }
        if ((ret = curl_easy_setopt(curl, CURLOPT_WRITEDATA, 
                    &stream)) != CURLE_OK) {
            EXO_CURL_ERROR("curl_easy_setopt(CURLOPT_WRITEDATA)", ret);
            continue;
        }
    
        if ((ret = curl_easy_perform(curl)) != CURLE_OK) {
            EXO_CURL_ERROR("curl_easy_perform", ret);
            continue;
        }
    
        auto str = stream.str();
        try {
            return cfg::parseFromMemory(str.begin(), str.end());
        } catch (const std::exception& e) {
            EXO_LOG("could not parse response as JSON, ignoring: %s", e.what());
            continue;
        }
    }

    exo::quit(exo::QuitStatus::NO_MORE_COMMANDS);
    return {};
}

void exo::HttpGetReadQueue::close() {
    closed_ = true;
}

exo::HttpPostWriteQueue::HttpPostWriteQueue(const exo::ConfigObject& config,
                                            const std::string& instanceId):
                HttpClient(config, instanceId) { }

std::ostream& exo::HttpPostWriteQueue::write() {
    return buffer_;
}

void exo::HttpPostWriteQueue::writeLine() {
    auto str = std::move(buffer_).str();

    CURLSListPtr slistPtr;
    CURLPtr curlPtr = exo::getCurl();
    auto curl = curlPtr.get();

    bool ok = exo::copyUrlHeaders(*this, curlPtr, slistPtr,
                                  staticHeadersPost,
                                  exo::arraySize(staticHeadersPost));
    if (!ok) return;

    CURLcode ret;
    std::ostringstream stream;
    if ((ret = curl_easy_setopt(curl, CURLOPT_POST, 1)) != CURLE_OK) {
        EXO_CURL_ERROR("curl_easy_setopt(CURLOPT_POST)", ret);
        return;
    }
    if ((ret = curl_easy_setopt(curl, CURLOPT_POSTFIELDS, 
                str.c_str())) != CURLE_OK) {
        EXO_CURL_ERROR("curl_easy_setopt(CURLOPT_POSTFIELDS)", ret);
        return;
    }

    if ((ret = curl_easy_perform(curl)) != CURLE_OK) {
        EXO_CURL_ERROR("curl_easy_perform", ret);
        return;
    }
}

} // namespace exo
