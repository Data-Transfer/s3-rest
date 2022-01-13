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
#include "webclient.h"

#include <algorithm>
#include <iostream>

#include "common.h"

namespace sss {

using namespace std;
std::atomic<int> WebClient::numInstances_{0};
std::mutex WebClient::cleanupMutex_;

size_t ReadFile(void* ptr, size_t size, size_t nmemb, void* userdata) {
    FILE* f = static_cast<FILE*>(userdata);
    if (ferror(f)) return CURL_READFUNC_ABORT;
    return fread(ptr, size, nmemb, f) * size;
}

size_t ReadFileUnbuffered(void* ptr, size_t size, size_t nmemb,
                          void* userdata) {
    const int fd = *static_cast<int*>(userdata);
    return max(ssize_t(0), read(fd, ptr, size * nmemb));
}

size_t WriteFile(char* ptr, size_t size, size_t nmemb, void* userdata) {
    FILE* writehere = static_cast<FILE*>(userdata);
    size = size * nmemb;
    fwrite(ptr, size, nmemb, writehere);
    return size;
}

size_t WriteFileUnbuffered(char* ptr, size_t size, size_t nmemb,
                           void* userdata) {
    const int fd = *static_cast<int*>(userdata);
    return max(ssize_t(0), write(fd, ptr, size * nmemb));
}

// public:
WebClient::~WebClient() {
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
bool WebClient::Send() {
    const bool ret = Status(curl_easy_perform(curl_));
    curl_easy_getinfo(curl_, CURLINFO_RESPONSE_CODE, &responseCode_);
    return ret;
}
bool WebClient::SetUrl(const std::string& url) {
    url_ = url;
    return curl_easy_setopt(curl_, CURLOPT_URL, url_.c_str()) == CURLE_OK;
}
void WebClient::SetEndpoint(const std::string& ep) {
    endpoint_ = ep;
    BuildURL();
}
void WebClient::SetPath(const std::string& path) {
    path_ = path;
    BuildURL();
}

bool WebClient::SSLVerify(bool verifyPeer, bool verifyHost) {
    return Status(curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYPEER,
                                   verifyPeer ? 1L : 0L)) &&
           ///@warning insecure!!!
           Status(curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYHOST,
                                   verifyHost ? 1L : 0L));
}

void WebClient::SetHeaders(const Map& headers) {
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
void WebClient::SetReqParameters(const Map& params) {
    params_ = params;
    BuildURL();
}
void WebClient::SetMethod(const std::string& method, size_t size) {
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

void WebClient::SetPostData(const std::string& data) {
    curl_easy_setopt(curl_, CURLOPT_POSTFIELDSIZE, data.size());
    curl_easy_setopt(curl_, CURLOPT_COPYPOSTFIELDS, data.c_str());
}

long WebClient::StatusCode() const { return responseCode_; }
const std::string& WebClient::GetUrl() const { return url_; }
const std::vector<uint8_t>& WebClient::GetResponse() const {
    return writeBuffer_.data;
}
const std::vector<uint8_t>& WebClient::GetHeader() const {
    return headerBuffer_;
}

void WebClient::SetUploadData(const std::vector<uint8_t>& data) {
    readBuffer_.data = data;
    readBuffer_.offset = 0;
}
bool WebClient::UploadFile(const std::string& fname, size_t fsize) {
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
bool WebClient::UploadDataFromBuffer(const char* data, size_t offset,
                                     size_t size) {
    if (curl_easy_setopt(curl_, CURLOPT_READFUNCTION, BufferReader) !=
        CURLE_OK) {
        throw std::runtime_error("Cannot set curl read function");
    }
    if (curl_easy_setopt(curl_, CURLOPT_READDATA, &refBuffer_) != CURLE_OK) {
        throw std::runtime_error("Cannot set curl read data buffer");
    }
    refBuffer_.data = data;
    refBuffer_.offset = offset;
    refBuffer_.size = size;
    SetMethod("PUT", size);
    return Send();
}
bool WebClient::UploadFile(const std::string& fname, size_t offset,
                           size_t size) {
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
bool WebClient::UploadFileUnbuffered(const std::string& fname, size_t offset,
                                     size_t size) {
    int file = open(fname.c_str(), S_IRGRP | O_LARGEFILE);
    if (file < 0) {
        throw std::runtime_error(strerror(errno));
    }
    if (lseek(file, offset, SEEK_SET) < 0) {
        throw std::runtime_error(strerror(errno));
    }
    if (!SetReadFunction(ReadFileUnbuffered, &file)) {
        throw std::runtime_error("Cannot set read function");
    }
    SetMethod("PUT", size);
    const bool result = Send();
    if (!result) {
        throw std::runtime_error("Error sending request: " + ErrorMsg());
        close(file);
    }
    close(file);
    return result;
}
bool WebClient::UploadFileMM(const std::string& fname, size_t offset,
                             size_t size) {
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
std::string WebClient::ErrorMsg() const { return errorBuffer_.data(); }
CURLcode WebClient::SetOpt(CURLoption option, va_list argp) {
    return curl_easy_setopt(curl_, option, argp);
}
CURLcode WebClient::GetInfo(CURLINFO info, va_list argp) {
    return curl_easy_getinfo(curl_, info, argp);
}
void WebClient::SetVerbose(bool verbose) {
    curl_easy_setopt(curl_, CURLOPT_VERBOSE, verbose ? 1L : 0);
}
std::string WebClient::GetContentText() const {
    std::vector<uint8_t> content = GetResponse();
    return std::string(begin(content), end(content));
}
std::string WebClient::GetHeaderText() const {
    std::vector<uint8_t> header = GetHeader();
    return std::string(begin(header), end(header));
}

// private:
bool WebClient::Status(CURLcode cc) const {
    if (cc == 0) return true;
    // deal with "SSL_write() returned SYSCALL, errno = 32"
    if (cc == CURLE_SEND_ERROR) {
        const std::string err(begin(errorBuffer_), end(errorBuffer_));
        if (err.find("32") != std::string::npos) return true;
        if (ToUpper(err).find("PIPE") != std::string::npos) return true;
    }
    return false;
}
void WebClient::InitEnv() {
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
bool WebClient::Init() {
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
    if (curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &writeBuffer_) != CURLE_OK)
        goto handle_error;
    if (curl_easy_setopt(curl_, CURLOPT_READFUNCTION, Reader) != CURLE_OK)
        goto handle_error;
    if (curl_easy_setopt(curl_, CURLOPT_READDATA, &readBuffer_) != CURLE_OK)
        goto handle_error;
    if (curl_easy_setopt(curl_, CURLOPT_HEADERFUNCTION, HeaderWriter) !=
        CURLE_OK)
        goto handle_error;
    if (curl_easy_setopt(curl_, CURLOPT_HEADERDATA, &headerBuffer_) != CURLE_OK)
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
    signal(SIGPIPE, SIG_IGN);
    curl_easy_setopt(curl_, CURLOPT_NOSIGNAL, 1L);
    return true;
handle_error:
    throw(std::runtime_error(errorBuffer_.data()));
    return false;
}
bool WebClient::BuildURL() {
    url_ = endpoint_ + path_;
    if (!params_.empty()) {
        url_ += "?" + UrlEncode(params_);
    }
    return SetUrl(url_);
}
size_t WebClient::Writer(char* data, size_t size, size_t nmemb,
                         Buffer* outbuffer) {
    assert(outbuffer);
    size = size * nmemb;
    outbuffer->data.insert(begin(outbuffer->data) + outbuffer->offset,
                           (uint8_t*)data, (uint8_t*)data + size);
    outbuffer->offset += size;
    return size;
}
size_t WebClient::HeaderWriter(char* data, size_t size, size_t nmemb,
                               std::vector<uint8_t>* writerData) {
    assert(writerData);
    writerData->insert(writerData->end(), (uint8_t*)data,
                       (uint8_t*)data + size * nmemb);
    return size * nmemb;
}
size_t WebClient::Reader(void* ptr, size_t size, size_t nmemb,
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
size_t WebClient::BufferReader(void* ptr, size_t size, size_t nmemb,
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
}  // namespace sss