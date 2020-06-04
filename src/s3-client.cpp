
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
 *******************************************************************************/

// Send S3v4 signed REST requests: to be finished then split into separate files

#include <aws_sign.h>
#include <curl/curl.h>
#include <curl/easy.h>
#include <sys/stat.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "lyra/lyra.hpp"
#include "url_utility.h"

using namespace std;


// TODO: change configuration when method changes, add option to specify
// read/write callbacks, write UrlEncode(map) using curl's url encode function
// curl_easy_escape(curl, "data to convert", 15);
class WebRequest {
    using WriteFunction = size_t (*)(char* data, size_t size, size_t nmemb,
                                     void* writerData);
    using ReadFunction = size_t (*)(void* ptr, size_t size, size_t nmemb,
                                    void* userdata);

    struct Buffer {
        size_t offset = 0;     // pointer to next insertion point
        vector<uint8_t> data;  // buffer
    };

   public:
    WebRequest(const WebRequest&) = delete;
    WebRequest(WebRequest&& other)
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
        if (curl_) {  // current object could have been moved
            // libcurl init and cleanup functions ar NOT thread safe and the
            // behaviour is unpredictable in case of use in a multithreaded
            // environment, this just guarantees that there is only a single
            // call to curl_global_cleanup
            curl_easy_cleanup(curl_);
            const std::lock_guard<std::mutex> lock(cleanupMutex_);
            --numInstances_;
            if(numInstances_ == 0)
                curl_global_cleanup();
        }
    }
    bool Send() {
        const bool ok = curl_easy_perform(curl_) == CURLE_OK;
        if (ok) {
            curl_easy_getinfo(curl_, CURLINFO_RESPONSE_CODE, &responseCode_);
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
    void SetReqParameters(const map<string, string>& params) {
        params_ = params;
        BuildURL();
    }
    void SetMethod(const string& method, size_t size = 0) {
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
            curl_easy_setopt(curl_, CURLOPT_INFILESIZE, size);
            curl_easy_setopt(curl_, CURLOPT_CUSTOMREQUEST, "PUT");
        }
    }
    template <typename T>
    void SetPostData(const T& postData) {  // can be dict or string or
                                           // anything for which an
                                           // urlencode function exists
        urlEncodedPostData_ = UrlEncode(postData);
    }

    long StatusCode() const { return responseCode_; }
    const vector<uint8_t>& GetContent() const { return writeBuffer_.data; }
    const vector<uint8_t>& GetHeader() const { return headerBuffer_; }

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
    void SetUploadData(const vector<uint8_t>& data) {
        readBuffer_.data = data;
        readBuffer_.offset = 0;
    }

    bool UploadFile(const string& fname) {
        struct stat st;
        if (stat(fname.c_str(), &st)) return -1;
        const size_t size = st.st_size;
        FILE* file = fopen(fname.c_str(), "rb");
        SetReadFunction(NULL, file);
        SetMethod("PUT", size);
        const bool result = Send();
        fclose(file);
        return result;
    }
    string ErrorMsg() const { return errorBuffer_.data(); }
    CURLcode SetOpt(CURLoption option, va_list argp) {
        return curl_easy_setopt(curl_, option, argp);
    }
    CURLcode GetInfo(CURLINFO info, va_list argp) {
        return curl_easy_getinfo(curl_, info, argp);
    }

   private:
    void InitEnv() {
        // first thread initializes curl the others wait until
        // initialization is complete
        // NOTE: libcurl initialization and cleanup are not thread safe,
        // this code just guarantees that curl_global_init is called ony
        // once TODO: with lock and numInstances_ variable globalInit
        // useless --> remove
        const std::lock_guard<std::mutex> lock(cleanupMutex_);
        ++numInstances_;
        const InitState prev = globalInit_.exchange(INITIALIZING);
        if (prev == UNINITIALIZED) {
            if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
                throw std::runtime_error("Cannot initialize libcurl");
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
        //curl_easy_setopt(curl_, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_0);
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
                         Buffer* outbuffer) {
        assert(outbuffer);
        size = size * nmemb;
        outbuffer->data.insert(begin(outbuffer->data) + outbuffer->offset,
                               (uint8_t*)data, (uint8_t*)data + size);
        outbuffer->offset += size;
        return size;
    }
    static size_t HeaderWriter(char* data, size_t size, size_t nmemb,
                               vector<uint8_t>* writerData) {
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

   private:
    CURL* curl_ = NULL;  // C pointer
    string url_;
    array<char, CURL_ERROR_SIZE> errorBuffer_;
    Buffer writeBuffer_;
    vector<uint8_t> headerBuffer_;
    string endpoint_;              // https://a.b.c:8080
    string path_;                  // /root/child1/child1.1
    map<string, string> headers_;  //{{"host", "myhost"},...} --> host: myhost
    map<string, string> params_;  //{{"key1", "val1"}, {"key2", "val2"},...} -->
                                  // key1=val1&key2=val2...
    string method_;               // GET | POST | PUT | HEAD | DELETE
    curl_slist* curlHeaderList_ = NULL;  // C struct --> NULL not nullptr
    long responseCode_ = 0;              // CURL uses a long type for status
    string urlEncodedPostData_;
    Buffer readBuffer_;

   private:
    enum InitState { UNINITIALIZED = 2, INITIALIZING = 1, INITIALIZED = 0 };
    static std::atomic<InitState> globalInit_;
    static std::atomic<int> numInstances_;
    static std::mutex cleanupMutex_;
};

std::atomic<WebRequest::InitState> WebRequest::globalInit_{UNINITIALIZED};
std::atomic<int> WebRequest::numInstances_{0};
std::mutex WebRequest::cleanupMutex_;

size_t WriteFS(char* data, size_t size, size_t nmemb, std::ofstream* os) {
    assert(os);
    assert(data);
    os->write(data, size * nmemb);
    return size * nmemb;
}

size_t ReadFS(void* out, size_t size, size_t nmemb, std::ifstream* is) {
    assert(out);
    assert(is);
    is->read(reinterpret_cast<char*>(out), nmemb * size);
    return is->gcount();
}

//------------------------------------------------------------------------------
struct Args {
    bool showHelp = false;
    string s3AccessKey;
    string s3SecretKey;
    string endpoint;
    string bucket;
    string key;
    string params;
    string method = "GET";
    string headers;
    string data;
    string outfile;
};

//------------------------------------------------------------------------------
void PrintArgs(const Args& args) {
    cout << "awsAccessKey: " << args.s3AccessKey << endl
         << "awsSecretKey: " << args.s3SecretKey << endl
         << "endpoint:     " << args.endpoint << endl
         << "method:       " << ToUpper(args.method) << endl
         << "bucket:       " << args.bucket << endl
         << "key:          " << args.key << endl
         << "parameters:   " << args.params << endl;
}

int main(int argc, char const* argv[]) {
    // The parser with the multiple option arguments and help option.
    Args args;
    auto cli =
        lyra::help(args.showHelp)
            .description("Send REST request with S3v4 signing") |
        lyra::opt(args.s3AccessKey,
                  "awsAccessKey")["-a"]["--access_key"]("AWS access key")
            .required() |
        lyra::opt(args.s3SecretKey,
                  "awsSecretKey")["-s"]["--secret_key"]("AWS secret key")
            .required() |
        lyra::opt(args.endpoint, "endpoint")["-e"]["--endpoint"]("Endpoing URL")
            .required() |
        lyra::opt(args.method, "method")["-m"]["--method"](
            "HTTP method: get | put | post | delete")
            .optional() |
        lyra::opt(args.params, "params")["-p"]["--params"](
            "URL request parameters. key1=value1;key2=...")
            .optional() |
        lyra::opt(args.bucket, "bucket")["-b"]["--bucket"]("Bucket name")
            .optional() |
        lyra::opt(args.key, "key")["-k"]["--key"]("Key name").optional() |
        lyra::opt(args.data, "content")["-d"]["--data"](
            "Data, use '@' prefix for file name")
            .optional() |
        lyra::opt(args.params, "headers")["-H"]["--headers"](
            "URL request headers. header1:value1;header2:...")
            .optional() |
        lyra::opt(args.outfile, "output")["-o"]["--out-file"]("output file");
    // Parse the program arguments:
    auto result = cli.parse({argc, argv});
    if (!result) {
        cerr << result.errorMessage() << endl;
        cerr << cli << endl;
        exit(1);
    }
    if (args.showHelp) {
        cout << cli;
        return 0;
    }
    string path;
    if (!args.bucket.empty()) {
        path += "/" + args.bucket;
        if (!args.key.empty()) path += "/" + args.key;
    }
    auto headers =
        SignHeaders(args.s3AccessKey, args.s3SecretKey, args.endpoint,
                    args.method, args.bucket, args.key);
    WebRequest req(args.endpoint, path, args.method, map<string, string>(),
                   headers);
    FILE* of = NULL;
    if (!args.outfile.empty()) {
        of = fopen(args.outfile.c_str(), "wb");
        req.SetWriteFunction(NULL, of);
    }
    if (!args.data.empty()) {
        if (args.data[0] != '\\') {
            req.SetUploadData(
                vector<uint8_t>(begin(args.data), end(args.data)));
            req.SetMethod("PUT", args.data.size());
            req.Send();
            req.StatusCode();
        } else {
            req.UploadFile(args.data.substr(1));
        }
    } else
        req.Send();
    if (of) fclose(of);
    cout << "Status: " << req.StatusCode() << endl;
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
