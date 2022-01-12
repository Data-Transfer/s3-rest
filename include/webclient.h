/*******************************************************************************
 * BSD 3-Clause License
 *
 * Copyright (c) 2020, Ugo Varetto
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *POSSIBILITY OF SUCH DAMAGE.
 ******************************************************************************/
#pragma once

#include <curl/curl.h>
#include <curl/easy.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <array>
#include <atomic>
#include <cassert>
#include <cstring>
#include <fstream>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>
//#ifdef IGNORE_SIGPIPE REQUIRED!
#include <signal.h>
//#endif

#include "common.h"
#include "url_utility.h"
#include "utility.h"


class WebClient {
    using WriteFunction = size_t (*)(char* data, size_t size, size_t nmemb,
                                     void* writerData);
    using ReadFunction = size_t (*)(void* ptr, size_t size, size_t nmemb,
                                    void* userdata);

    struct Buffer {
        size_t offset = 0;          // pointer to next insertion point
        std::vector<uint8_t> data;  // buffer
    };

    struct ReadBuffer {
        size_t offset = 0;  // pointer to next insertion point
        const char* data;   // buffer
        size_t size = 0;    // buffer size
    };

   public:
    WebClient(const WebClient&) = delete;
    WebClient(WebClient&& other)
        : endpoint_(other.endpoint_),
          path_(other.path_),
          method_(other.method_),
          params_(other.params_),
          headers_(other.headers_),
          writeBuffer_(other.writeBuffer_),
          headerBuffer_(other.headerBuffer_),
          curl_(other.curl_) {
        other.curl_ = NULL;
    }
    WebClient() { InitEnv(); }
    WebClient(const std::string& url) : url_(url), method_("GET") { InitEnv(); }
    WebClient(const std::string& ep, const std::string& path,
              const std::string& method = "GET",
              const Map params =
                  Map(),
              const Map headers =
                  Map())
        : endpoint_(ep),
          path_(path),
          method_(method),
          params_(params),
          headers_(headers) {
        InitEnv();
    }
    ~WebClient();
    bool Send();
    bool SetUrl(const std::string& url);
    void SetEndpoint(const std::string& ep);
    void SetPath(const std::string& path);
    void SetHeaders(const Map& headers);
    void SetReqParameters(const Map& params);
    void SetMethod(const std::string& method, size_t size = 0);
    template <typename T>
    void SetUrlEncodedPostData(
        const T& postData) {  // can be dict or std::string or
                              // anything for which an
                              // urlencode function exists
        urlEncodedPostData_ = UrlEncode(postData);
        curl_easy_setopt(curl_, CURLOPT_POSTFIELDSIZE,
                         urlEncodedPostData_.size());
        curl_easy_setopt(curl_, CURLOPT_COPYPOSTFIELDS,
                         urlEncodedPostData_.c_str());
    }

    void SetPostData(const std::string& data);

    template <typename T>
    void SetRawPostData(const T& data, size_t size) {
        curl_easy_setopt(curl_, CURLOPT_POSTFIELDSIZE, size);
        curl_easy_setopt(curl_, CURLOPT_POSTFIELDSIZE_LARGE, size);
    }

    long StatusCode() const;
    const std::string& GetUrl() const;
    const std::vector<uint8_t>& GetContent() const;
    const std::vector<uint8_t>& GetHeader() const;

    template <typename T>
    bool SetWriteFunction(WriteFunction f, T* ptr) {
        if (curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, f) != CURLE_OK)
            return false;
        if (curl_easy_setopt(curl_, CURLOPT_WRITEDATA, ptr) != CURLE_OK)
            return false;
        return true;
    }
    template <typename T>
    bool SetReadFunction(ReadFunction f, T* ptr) {
        if (curl_easy_setopt(curl_, CURLOPT_READFUNCTION, f) != CURLE_OK)
            return false;
        if (curl_easy_setopt(curl_, CURLOPT_READDATA, ptr) != CURLE_OK)
            return false;
        return true;
    }
    void SetUploadData(const std::vector<uint8_t>& data);

    bool UploadFile(const std::string& fname, size_t fsize = 0);
    bool UploadDataFromBuffer(const char* data, size_t offset, size_t size);
    bool UploadFile(const std::string& fname, size_t offset, size_t size);

    bool UploadFileUnbuffered(const std::string& fname, size_t offset,
                              size_t size);

    bool UploadFileMM(const std::string& fname, size_t offset, size_t size);

    std::string ErrorMsg() const;
    CURLcode SetOpt(CURLoption option, va_list argp);
    CURLcode GetInfo(CURLINFO info, va_list argp);
    void SetVerbose(bool verbose);
    std::string GetContentText() const;
    std::string GetHeaderText() const;

   private:
    bool Status(CURLcode cc) const;
    void InitEnv();
    bool Init();
    void BuildURL();
    static size_t Writer(char* data, size_t size, size_t nmemb,
                         Buffer* outbuffer);
    static size_t HeaderWriter(char* data, size_t size, size_t nmemb,
                               std::vector<uint8_t>* writerData);
    static size_t Reader(void* ptr, size_t size, size_t nmemb,
                         Buffer* inbuffer);
    static size_t BufferReader(void* ptr, size_t size, size_t nmemb,
                               ReadBuffer* inbuffer);
   private:
    CURL* curl_ = NULL;  // C pointer
    std::string url_;
    std::array<char, CURL_ERROR_SIZE> errorBuffer_;
    Buffer writeBuffer_;
    std::vector<uint8_t> headerBuffer_;
    std::string endpoint_;  // https://a.b.c:8080
    std::string path_;      // /root/child1/child1.1
    Map headers_;  //{{"host", "myhost"},...} --> host: myhost
    Map params_;          //{{"key1", "val1"}, {"key2", "val2"},...} -->
                          // key1=val1&key2=val2...
    std::string method_;  // GET | POST | PUT | HEAD | DELETE
    curl_slist* curlHeaderList_ = NULL;  // C struct --> NULL not nullptr
    long responseCode_ = 0;              // CURL uses a long type for status
    std::string urlEncodedPostData_;
    Buffer readBuffer_;
    ReadBuffer refBuffer_;

   private:
    static std::atomic<int> numInstances_;
    static std::mutex cleanupMutex_;
};
