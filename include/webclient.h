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
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>

#include <array>
#include <atomic>
#include <cassert>
#include <fstream>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef IGNORE_SIGPIPE
#include <signal.h>
#endif

#include "url_utility.h"
#include "utility.h"

size_t ReadFile(void* ptr, size_t size, size_t nmemb, void* userdata);

size_t WriteFile(char* data, size_t size, size_t nmemb, void* outbuffer);

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
              const std::map<std::string, std::string> params =
                  std::map<std::string, std::string>(),
              const std::map<std::string, std::string> headers =
                  std::map<std::string, std::string>())
        : endpoint_(ep),
          path_(path),
          method_(method),
          params_(params),
          headers_(headers) {
        InitEnv();
    }
    ~WebClient() {
        if (curlHeaderList_) {
            curl_slist_free_all(curlHeaderList_);
        }
        if (curl_) {  // current object could have been moved
            // libcurl init and cleanup functions ar NOT thread safe and the
            // behaviour is unpredictable in case of use in a multithreaded
            // environment, this just guarantees that there is only a single
            // call to curl_global_cleanup
            curl_easy_cleanup(curl_);
            const std::lock_guard<std::mutex> lock(cleanupMutex_);
            --numInstances_;
            if (numInstances_ == 0) curl_global_cleanup();
        }
    }
    bool Send() {
        const bool ret = Status(curl_easy_perform(curl_));
        curl_easy_getinfo(curl_, CURLINFO_RESPONSE_CODE, &responseCode_);
        return ret;
    }
    bool SetUrl(const std::string& url) {
        url_ = url;
        return curl_easy_setopt(curl_, CURLOPT_URL, url_.c_str()) == CURLE_OK;
    }
    void SetEndpoint(const std::string& ep) {
        endpoint_ = ep;
        BuildURL();
    }
    void SetPath(const std::string& path) {
        path_ = path;
        BuildURL();
    }
    void SetHeaders(const std::map<std::string, std::string>& headers) {
        headers_ = headers;
        if (curlHeaderList_) {
            curl_slist_free_all(curlHeaderList_);
            curlHeaderList_ = NULL;
        }
        if (!headers_.empty()) {
            for (auto kv : headers_) {
                curlHeaderList_ = curl_slist_append(
                    curlHeaderList_, (kv.first + ": " + kv.second).c_str());
            }
        }
        curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, curlHeaderList_);
    }
    void SetReqParameters(const std::map<std::string, std::string>& params) {
        params_ = params;
        BuildURL();
    }
    void SetMethod(const std::string& method, size_t size = 0) {
        method_ = ToUpper(method);
        if (method_ == "GET") {
            curl_easy_setopt(curl_, CURLOPT_NOBODY, 0L);
            curl_easy_setopt(curl_, CURLOPT_HTTPGET, 1L);
        } else if (method == "HEAD") {
            curl_easy_setopt(curl_, CURLOPT_NOBODY, 1L);
            curl_easy_setopt(curl_, CURLOPT_CUSTOMREQUEST, "HEAD");
        } else if (method == "DELETE") {
            curl_easy_setopt(curl_, CURLOPT_CUSTOMREQUEST, "DELETE");
        } else if (method == "POST") {
            curl_easy_setopt(curl_, CURLOPT_CUSTOMREQUEST, "POST");
            curl_easy_setopt(curl_, CURLOPT_POSTFIELDSIZE,
                             urlEncodedPostData_.size());
            curl_easy_setopt(curl_, CURLOPT_POSTFIELDS,
                             urlEncodedPostData_.c_str());
        } else if (method_ == "PUT") {
            curl_easy_setopt(curl_, CURLOPT_UPLOAD, 1L);
            curl_easy_setopt(curl_, CURLOPT_INFILESIZE_LARGE, size);
            curl_easy_setopt(curl_, CURLOPT_CUSTOMREQUEST, "PUT");
        }
    }
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

    void SetPostData(const std::string& data) {
        curl_easy_setopt(curl_, CURLOPT_POSTFIELDSIZE, data.size());
        curl_easy_setopt(curl_, CURLOPT_COPYPOSTFIELDS, data.c_str());
    }

    template <typename T>
    void SetRawPostData(const T& data, size_t size) {
        curl_easy_setopt(curl_, CURLOPT_POSTFIELDSIZE, size);
        curl_easy_setopt(curl_, CURLOPT_POSTFIELDSIZE_LARGE, size);
    }

    long StatusCode() const { return responseCode_; }
    const std::string& GetUrl() const { return url_; }
    const std::vector<uint8_t>& GetContent() const { return writeBuffer_.data; }
    const std::vector<uint8_t>& GetHeader() const { return headerBuffer_; }

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
    void SetUploadData(const std::vector<uint8_t>& data) {
        readBuffer_.data = data;
        readBuffer_.offset = 0;
    }

    bool UploadFile(const std::string& fname, size_t fsize = 0) {
        const size_t size = fsize ? fsize : FileSize(fname);
        FILE* file = fopen(fname.c_str(), "rb");
        if (!file) {
            throw std::runtime_error("Cannot open file " + fname);
        }
        if (!SetReadFunction(NULL, file)) {
            throw std::runtime_error("Cannot set read function");
        }
        SetMethod("PUT", size);
        const bool result = Send();
        if (!result) {
            throw std::runtime_error("Error sending request: " + ErrorMsg());
            fclose(file);
        }
        fclose(file);
        return result;
    }

    // bool UploadFileUnbuffered(const std::string& fname, size_t fsize) {
    //     const int flags = O_RDONLY | O_LARGEFILE;
    //     const int mode = S_IRUSR;  // | S_IWUSR | S_IRGRP | S_IROTH;
    //     const int fd = open(fname.c_str(), flags, mode);
        

    //     if (!SetReadFunction(NULL, file)) {
    //         throw std::runtime_error("Cannot set read function");
    //     }
    //     SetMethod("PUT", size);
    //     const bool result = Send();
    //     if (!result) {
    //         throw std::runtime_error("Error sending request: " + ErrorMsg());
    //         fclose(file);
    //     }
    //     close(fd);
    //     return result;
    // }

    bool UploadDataFromBuffer(const char* data, size_t offset, size_t size) {
#ifdef IGNORE_SIGPIPE
        signal(SIGPIPE, SIG_IGN);
        curl_easy_setopt(curl_, CURLOPT_NOSIGNAL, 1L);
#endif
        if (curl_easy_setopt(curl_, CURLOPT_READFUNCTION, BufferReader) !=
            CURLE_OK) {
            throw std::runtime_error("Cannot set curl read function");
        }
        if (curl_easy_setopt(curl_, CURLOPT_READDATA, &refBuffer_) !=
            CURLE_OK) {
            throw std::runtime_error("Cannot set curl read data buffer");
        }
        refBuffer_.data = data;
        refBuffer_.offset = offset;
        refBuffer_.size = size;
        SetMethod("PUT", size);
        return Send();
    }

    bool UploadFile(const std::string& fname, size_t offset, size_t size) {
#ifdef IGNORE_SIGPIPE
        signal(SIGPIPE, SIG_IGN);
        curl_easy_setopt(curl_, CURLOPT_NOSIGNAL, 1L);
#endif
        FILE* file = fopen(fname.c_str(), "rb");
        if (!file) {
            throw std::runtime_error("Cannot open file " + fname);
        }
        if (fseek(file, offset, SEEK_SET)) {
            throw std::runtime_error("Cannot move file pointer");
        }
        if (!SetReadFunction(ReadFile, file)) {
            throw std::runtime_error("Cannot set read function");
        }
        SetMethod("PUT", size);
        const bool result = Send();
        if (!result) {
            throw std::runtime_error("Error sending request: " + ErrorMsg());
            fclose(file);
        }
        fclose(file);
        return result;
    }

    bool UploadFileMM(const std::string& fname, size_t offset, size_t size) {
#ifdef IGNORE_SIGPIPE
        signal(SIGPIPE, SIG_IGN);
        curl_easy_setopt(curl_, CURLOPT_NOSIGNAL, 1L);
#endif
        int fd = open(fname.c_str(), O_RDONLY | O_LARGEFILE);
        if (fd < 0) {
            throw std::runtime_error("Error cannot open input file: " +
                                     std::string(strerror(errno)));
            exit(EXIT_FAILURE);
        }
      
        char* src = (char*)mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, offset);
        if (src == MAP_FAILED) {  // mmap returns (void *) -1 == MAP_FAILED
            throw std::runtime_error("Error mapping memory: " +
                                     std::string(strerror(errno)));
            exit(EXIT_FAILURE);
        }
        if (!UploadDataFromBuffer(src, 0, size)) {
            throw std::runtime_error("Error uploading memory mapped data");
            exit(EXIT_FAILURE);
        }
        
        if (munmap(src, size)) {
            throw std::runtime_error("Error unmapping memory: " +
                                     std::string(strerror(errno)));
            exit(EXIT_FAILURE);
        }
        if (close(fd)) {
            throw std::runtime_error("Error closing file" +
                                     std::string(strerror(errno)));
            exit(EXIT_FAILURE);
        }
        return true;
    }

    std::string ErrorMsg() const { return errorBuffer_.data(); }
    CURLcode SetOpt(CURLoption option, va_list argp) {
        return curl_easy_setopt(curl_, option, argp);
    }
    CURLcode GetInfo(CURLINFO info, va_list argp) {
        return curl_easy_getinfo(curl_, info, argp);
    }

    void SetVerbose(bool verbose) {
        curl_easy_setopt(curl_, CURLOPT_VERBOSE, verbose ? 1L : 0);
    }

    std::string GetContentText() const {
        std::vector<uint8_t> content = GetContent();
        return std::string(begin(content), end(content));
    }

    std::string GetHeaderText() const {
        std::vector<uint8_t> header = GetHeader();
        return std::string(begin(header), end(header));
    }

   private:
    bool Status(CURLcode cc) const {
        if (cc == 0) return true;
        // deal with SIGPIPE
        if (cc == CURLE_SEND_ERROR) {
            const std::string err(begin(errorBuffer_), end(errorBuffer_));
            if (err.find("pipe") != std::string::npos) return true;
        }
        return false;
    }
    void InitEnv() {
        // first thread initializes curl the others wait until
        // initialization is complete
        // NOTE: libcurl initialization and cleanup are not thread safe,
        // this code just guarantees that curl_global_init is called ony
        // once TODO: with lock and numInstances_ variable globalInit
        // useless --> remove
        const std::lock_guard<std::mutex> lock(cleanupMutex_);
        if (numInstances_ == 0) {
            if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
                throw std::runtime_error("Cannot initialize libcurl");
            }
        }
        ++numInstances_;
        Init();
    }
    bool Init() {
        curl_ = curl_easy_init();
        // curl_easy_setopt(curl_, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_0);
        if (!curl_) {
            throw(std::runtime_error("Cannot create Curl connection"));
            return false;
        }
        if (curl_easy_setopt(curl_, CURLOPT_ERRORBUFFER, errorBuffer_.data()) !=
            CURLE_OK)
            goto handle_error;
        if (curl_easy_setopt(curl_, CURLOPT_FOLLOWLOCATION, 1L) != CURLE_OK)
            goto handle_error;
        if (curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, Writer) != CURLE_OK)
            goto handle_error;
        if (curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &writeBuffer_) !=
            CURLE_OK)
            goto handle_error;
        if (curl_easy_setopt(curl_, CURLOPT_READFUNCTION, Reader) != CURLE_OK)
            goto handle_error;
        if (curl_easy_setopt(curl_, CURLOPT_READDATA, &readBuffer_) != CURLE_OK)
            goto handle_error;
        if (curl_easy_setopt(curl_, CURLOPT_HEADERFUNCTION, HeaderWriter) !=
            CURLE_OK)
            goto handle_error;
        if (curl_easy_setopt(curl_, CURLOPT_HEADERDATA, &headerBuffer_) !=
            CURLE_OK)
            goto handle_error;
        if (curl_easy_setopt(curl_, CURLOPT_NOSIGNAL, 1L) != CURLE_OK) {
            goto handle_error;
        }
        if (curl_easy_setopt(curl_, CURLOPT_NOPROGRESS, 1L) != CURLE_OK) {
            goto handle_error;
        };
        if (method_.size()) {
            SetMethod(method_);
        }
        if (!headers_.empty()) {
            SetHeaders(headers_);
        }
        if (!params_.empty()) {
            SetReqParameters(params_);
        }
        if (!endpoint_.empty()) {
            BuildURL();
        }
        return true;
    handle_error:
        throw(std::runtime_error(errorBuffer_.data()));
        return false;
    }
    void BuildURL() {
        url_ = endpoint_ + path_;
        if (!params_.empty()) {
            url_ += "?" + UrlEncode(params_);
        }
        SetUrl(url_);
    }
    static size_t Writer(char* data, size_t size, size_t nmemb,
                         Buffer* outbuffer) {
        assert(outbuffer);
        size = size * nmemb;
        outbuffer->data.insert(begin(outbuffer->data) + outbuffer->offset,
                               (uint8_t*)data, (uint8_t*)data + size);
        outbuffer->offset += size;
        return size;
    }
    static size_t HeaderWriter(char* data, size_t size, size_t nmemb,
                               std::vector<uint8_t>* writerData) {
        assert(writerData);
        writerData->insert(writerData->end(), (uint8_t*)data,
                           (uint8_t*)data + size * nmemb);
        return size * nmemb;
    }
    static size_t Reader(void* ptr, size_t size, size_t nmemb,
                         Buffer* inbuffer) {
        const auto b = begin(inbuffer->data) + inbuffer->offset;
        if (b >= end(inbuffer->data)) {
            return 0;
        }
        size = size * nmemb;
        const auto e = std::min(b + size, end(inbuffer->data));
        std::copy(b, e, (uint8_t*)ptr);
        size = size_t(e - b);
        inbuffer->offset += size;
        return size;
    }
    static size_t BufferReader(void* ptr, size_t size, size_t nmemb,
                               ReadBuffer* inbuffer) {
        const auto b = inbuffer->data + inbuffer->offset;
        const auto end = inbuffer->data + inbuffer->offset + inbuffer->size;
        if (b >= end) {
            return 0;
        }
        size = size * nmemb;
        const auto e = std::min(b + size, end);
        std::copy(b, e, (char*)ptr);
        size = size_t(e - b);
        inbuffer->offset += size;
        return size;
    }

   private:
    CURL* curl_ = NULL;  // C pointer
    std::string url_;
    std::array<char, CURL_ERROR_SIZE> errorBuffer_;
    Buffer writeBuffer_;
    std::vector<uint8_t> headerBuffer_;
    std::string endpoint_;  // https://a.b.c:8080
    std::string path_;      // /root/child1/child1.1
    std::map<std::string, std::string>
        headers_;  //{{"host", "myhost"},...} --> host: myhost
    std::map<std::string, std::string>
        params_;          //{{"key1", "val1"}, {"key2", "val2"},...} -->
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
