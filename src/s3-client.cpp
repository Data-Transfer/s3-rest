//!!!!!!!! IN PROGRESS DO NOT TOUCH

/*******************************************************************************
* BSD 3-Clause License
* 
* Copyright (c) 2020, Ugo Varetto
* All rights reserved.
* 
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
* 
* 1. Redistributions of source code must retain the above copyright notice, this
*    list of conditions and the following disclaimer.
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
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
* SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
* CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*******************************************************************************/

//Send S3v4 signed REST requests: to be finished then split into separate files
// DO NOT TOUCH

#include <aws_sign.h>
#include <curl/curl.h>
#include <curl/easy.h>

#include <array>
#include <atomic>
#include <cassert>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "url_utility.h"

using namespace std;

// TODO: change configuration when method changes, add option to specify
// read/write callbacks, write UrlEncode(map) using curl's url encode function
// curl_easy_escape(curl, "data to convert", 15);
class WebRequest {
   public:
    WebRequest() { InitEnv(); }
    WebRequest(const string& url) : url_(url), method_("GET") { InitEnv(); }
    WebRequest(const string& ep, const string& path,
               const string& method = "GET",
               const map<string, string> params = map<string, string>(),
               const map<string, string> headers = map<string, string>())
        : endpoint_(ep),
          path_(path),
          method_(method),
          params_(params),
          headers_(headers) {
        InitEnv();
    }
    ~WebRequest() {
        if (curlHeaderList_) {
            curl_slist_free_all(curlHeaderList_);
        }
        curl_easy_cleanup(curl_);
        //need to call global_cleanup: re-implement thread safe code
        //like done for init
    }
    bool Send() {
        const bool ok = curl_easy_perform(curl_) == CURLE_OK;
        if (ok) {
            curl_easy_getinfo(curl_, CURLINFO_RESPONSE_CODE, responseCode_);
        } else {
            responseCode_ = 0;
        }
        return ok;
    }
    bool SetUrl(const string& url) {
        url_ = url;
        return curl_easy_setopt(curl_, CURLOPT_URL, url_.c_str()) == CURLE_OK;
    }
    void SetEndpoint(const string& ep) {
        endpoint_ = ep;
        BuildURL();
    }
    void SetPath(const string& path) {
        path_ = path;
        BuildURL();
    }
    void SetHeaders(const map<string, string>& headers) {
        headers_ = headers;
        if (curlHeaderList_) {
            curl_slist_free_all(curlHeaderList_);
        }
        if (!headers_.empty()) {
            for (auto kv : headers_) {
                curlHeaderList_ = curl_slist_append(
                    curlHeaderList_, (kv.first + ": " + kv.second).c_str());
            }
        }
        curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, curlHeaderList_);
    }
    void SetReqParameters(const map<string, string>& params) {
        params_ = params;
        BuildURL();
    }
    void SetMethod(const string& method) {
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
            curl_easy_setopt(curl_, CURLOPT_POSTFIELDSIZE,
                             urlEncodedPostData_.size());
            curl_easy_setopt(curl_, CURLOPT_POSTFIELDS,
                             urlEncodedPostData_.c_str());
        } else if (method_ == "PUT") {
            throw "PUT NOT IMPLEMENTED";
        }
    }
    void SetPostData(const map<string, string>& postData) {
        urlEncodedPostData_ = UrlEncode(postData);
    }
    long StatusCode() const { return responseCode_; }
    const vector<uint8_t>& GetContent() const { return buffer_; }
    const vector<uint8_t>& GetHeader() const { return headerBuffer_; }

   private:
    void InitEnv() {
        // first thread initializes curl the others wait until
        // initialization is complete
        const InitState prev = globalInit_.exchange(INITIALIZING);
        if (prev == UNINITIALIZED) {
            if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
            }
            globalInit_.store(INITIALIZED);
        } else {
            // C++ 20: can use wait, busy loop instead
            while (globalInit_.load() != INITIALIZED)
                ;
        }
        Init();
        if (!endpoint_.empty())
            BuildURL();
        else
            SetUrl(url_);
    }
    bool Init() {
        curl_ = curl_easy_init();
        if (!curl_) {
            throw(runtime_error("Cannot create Curl connection"));
            return false;
        }
        if (curl_easy_setopt(curl_, CURLOPT_ERRORBUFFER, errorBuffer_.data()) !=
            CURLE_OK)
            goto handle_error;
        if (curl_easy_setopt(curl_, CURLOPT_FOLLOWLOCATION, 1L) != CURLE_OK)
            goto handle_error;
        if (curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, Writer) != CURLE_OK)
            goto handle_error;
        if (curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &buffer_) != CURLE_OK)
            goto handle_error;
        curl_easy_setopt(curl_, CURLOPT_HEADER, 1);
        if (curl_easy_setopt(curl_, CURLOPT_HEADERFUNCTION, HeaderWriter) !=
            CURLE_OK)
            goto handle_error;
        if (curl_easy_setopt(curl_, CURLOPT_HEADERDATA, &headerBuffer_) !=
            CURLE_OK)
            goto handle_error;
        if (method_.size()) {
            SetMethod(method_);
        }
        if (!headers_.empty()) {
            SetHeaders(headers_);
        }
        return true;
    handle_error:
        throw(runtime_error(errorBuffer_.data()));
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
                         std::vector<uint8_t>* writerData) {
        assert(writerData);
        writerData->insert(writerData->end(), (uint8_t*)data,
                           (uint8_t*)data + size * nmemb);
        return size * nmemb;
    }
    static size_t HeaderWriter(char* data, size_t size, size_t nmemb,
                               std::vector<uint8_t>* writerData) {
        assert(writerData);
        writerData->insert(writerData->end(), (uint8_t*)data,
                           (uint8_t*)data + size * nmemb);
        return size * nmemb;
    }

   private:
    CURL* curl_;
    string url_;
    array<char, CURL_ERROR_SIZE> errorBuffer_;
    vector<uint8_t> buffer_;
    vector<uint8_t> headerBuffer_;
    string endpoint_;              // https://a.b.c:8080
    string path_;                  // /root/child1/child1.1
    map<string, string> headers_;  //{{"host", "myhost"},...} --> host: myhost
    map<string, string> params_;  //{{"key1", "val1"}, {"key2", "val2"},...} -->
                                  // key1=val1&key2=val2...
    string method_;               // GET | POST | PUT | HEAD | DELETE
    curl_slist* curlHeaderList_ = NULL;  // C struct --> NULL not nullptr
    long responseCode_ = 0;              // CURL uses a long type
    string urlEncodedPostData_;

   private:
    enum InitState { UNINITIALIZED = 2, INITIALIZING = 1, INITIALIZED = 0 };
    static atomic<InitState> globalInit_;
};

atomic<WebRequest::InitState> WebRequest::globalInit_(UNINITIALIZED);

int main(int argc, char const* argv[]) {
    const string path = "/uv-bucket-3/XXX";
    const string access_key = "00a5752015a64525bc45c55e88d2f162";
    const string bucket = "uv-bucket-3";
    const string endpoint = "https://nimbus.pawsey.org.au:8080";
    // const string endpoint = "http://localhost:8000";
    const string secret_key = "d1b8bdb35b7649deac055c3f77670f7f";
    const string method = "GET";
    auto headers =
        SignHeaders(access_key, secret_key, endpoint, method, bucket, "XXX");
    WebRequest req(endpoint, path, method, map<string, string>(), headers);
    const bool status = req.Send();
    vector<uint8_t> resp = req.GetContent();
    string t(begin(resp), end(resp));
    cout << t << endl;
    vector<uint8_t> h = req.GetHeader();
    string hs(begin(resp), end(resp));
    cout << hs << endl;
    return 0;
}

// HEADERS
// static size_t header_callback(char *buffer, size_t size,
//                               size_t nitems, void *userdata)
// {
//   /* received header is nitems * size long in 'buffer' NOT ZERO TERMINATED */
//   /* 'userdata' is set with CURLOPT_HEADERDATA */
//   return nitems * size;
// }

// CURL *curl = curl_easy_init();
// if(curl) {
//   curl_easy_setopt(curl, CURLOPT_URL, "http://example.com");

//   curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);

//   curl_easy_perform(curl);
// }

// STATUS CODE
// CURL *curl = curl_easy_init();
// if(curl) {
//   CURLcode res;
//   curl_easy_setopt(curl, CURLOPT_URL, "http://example.com");
//   res = curl_easy_perform(curl);
//   if(res == CURLE_OK) {
//     long response_code;
//     curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
//   }
//   curl_easy_cleanup(curl);
// }

// URLENCODE
// CURL *curl = curl_easy_init();
// if(curl) {
//   char *output = curl_easy_escape(curl, "data to convert", 15);
//   if(output) {
//     printf("Encoded: %s\n", output);
//     curl_free(output);
//   }
// }

// KEEP ALIVE
// CURL *curl = curl_easy_init();
// if(curl) {
//   curl_easy_setopt(curl, CURLOPT_URL, "http://example.com");

//   /* enable TCP keep-alive for this transfer */
//   curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);

//   /* keep-alive idle time to 120 seconds */
//   curl_easy_setopt(curl, CURLOPT_TCP_KEEPIDLE, 120L);

//   /* interval time between keep-alive probes: 60 seconds */
//   curl_easy_setopt(curl, CURLOPT_TCP_KEEPINTVL, 60L);

//   curl_easy_perform(curl);
// }

// POST URLENCODED FORM DATA
// CURL *curl = curl_easy_init();
// if(curl) {
//   const char *data = "data to send";

//   curl_easy_setopt(curl, CURLOPT_URL, "http://example.com");

//   /* size of the POST data */
//   curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, 12L);

//   /* pass in a pointer to the data - libcurl will not copy */
//   curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);

//   curl_easy_perform(curl);
// }

// UPLOAD FILE > 2GB WITH PUT
// CURL *curl = curl_easy_init();
// if(curl) {
//   /* we want to use our own read function */
//   curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_callback);

//   /* enable uploading */
//   curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);

//   /* specify target */
//   curl_easy_setopt(curl, CURLOPT_URL, "ftp://example.com/dir/to/newfile");

//   /* now specify which pointer to pass to our callback */
//   curl_easy_setopt(curl, CURLOPT_READDATA, hd_src);

//   /* Set the size of the file to upload */
//   curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)fsize);

//   /* Now run off and do what you've been told! */
//   curl_easy_perform(curl);
// }

// UPLOAD FILE <= 2GB
// CURL *curl = curl_easy_init();
// if(curl) {
//   long uploadsize = FILE_SIZE;

//   curl_easy_setopt(curl, CURLOPT_URL,
//   "ftp://example.com/destination.tar.gz");

//   curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);

//   curl_easy_setopt(curl, CURLOPT_INFILESIZE, uploadsize);

//   curl_easy_perform(curl);
// }
