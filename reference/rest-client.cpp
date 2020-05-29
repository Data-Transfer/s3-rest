#include <curl/curl.h>
#include <curl/easy.h>

#include <array>
#include <atomic>
#include <cassert>
#include <iostream>
#include <map>
#include <string>
#include <vector>

using namespace std;

//TODO: change configuration when method changes, add option to specify
//read/write callbacks, write UrlEncode(map) using curl's url encode function
//curl_easy_escape(curl, "data to convert", 15);
class WebRequest {
   public:
    WebRequest() { InitEnv(); }
    WebRequest(const string& url) : WebRequest(), url_(url), method_("GET") {
        InitEnv();
    }
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
    ~WebRequest() { curl_easy_cleanup(conn_); }
    bool Send() { return curl_easy_perform(conn_) == CURLE_OK; }
    bool SetUrl(const string& url) {
        url_ = url;
        return curl_easy_setopt(conn_, CURLOPT_URL, url_.c_str()) == CURLE_OK;
    }
    void SetEndpoint(const string& ep) {
        endpoint_ = ep;
        BuildURL();
    }
    void SetPath(const string& path) {
        path_ = path;
        BuildURL();
    }
    void SetHeaders(const map<string, string>& headers) { headers_ = headers; }
    void SetReqParameters(const map<string, string>& params) {
        params_ = params;
        BuildURL();
    }
    void SetMethod(const string& method) { method_ = method; }
    const vector<uint8_t>& GetContent() const { return buffer_; }

   private:
    void InitEnv() {
        const bool prev = globalInit_.exchange(true);
        if (!prev) {
            curl_global_init(CURL_GLOBAL_DEFAULT);
        }
        Init();
        if (!url_.empty())
            SetUrl(url_);
        else if (!endpoint_.empty())
            BuildURL();
    }
    bool Init() {
        conn_ = curl_easy_init();
        if (!conn_) {
            throw(runtime_error("Cannot create Curl connection"));
            return false;
        }
        if (curl_easy_setopt(conn_, CURLOPT_ERRORBUFFER, errorBuffer_.data()) !=
            CURLE_OK)
            goto handle_error;
        if (curl_easy_setopt(conn_, CURLOPT_FOLLOWLOCATION, 1L) != CURLE_OK)
            goto handle_error;
        if (curl_easy_setopt(conn_, CURLOPT_WRITEFUNCTION, Writer) != CURLE_OK)
            goto handle_error;
        if (curl_easy_setopt(conn_, CURLOPT_WRITEDATA, &buffer_) != CURLE_OK)
            goto handle_error;
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

   private:
    CURL* conn_;
    string url_;
    array<char, CURL_ERROR_SIZE> errorBuffer_;
    vector<uint8_t> buffer_;
    string endpoint_;              // https://a.b.c:8080
    string path_;                  // /root/child1/child1.1
    map<string, string> headers_;  //{{"host", "myhost"},...} --> host: myhost
    map<string, string> params_;  //{{"key1", "val1"}, {"key2", "val2"},...} -->
                                  // key1=val1&key2=val2...
    string method_;               // GET | POST | PUT | HEAD | DELETE

   private:
    static atomic<bool> globalInit_;
};

atomic<bool> WebRequest::globalInit_(false);

int main(int argc, char const* argv[]) {
    WebRequest req("https://www.pawsey.org.au");
    const bool status = req.Send();
    vector<uint8_t> resp = req.GetContent();
    string t(begin(resp), end(resp));
    cout << t << endl;
    req.SetUrl("https://google.com");
    req.Send();
    resp = req.GetContent();
    t = string(begin(resp), end(resp));
    cout << t << endl;
    return 0;
}


//HEADERS
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

//STATUS CODE
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

//URLENCODE
// CURL *curl = curl_easy_init();
// if(curl) {
//   char *output = curl_easy_escape(curl, "data to convert", 15);
//   if(output) {
//     printf("Encoded: %s\n", output);
//     curl_free(output);
//   }
// }

//KEEP ALIVE
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

//POST URLENCODED FORM DATA
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

//ADD CUSTOM HEADERS
// CURL *curl = curl_easy_init();
 
// struct curl_slist *list = NULL;
 
// if(curl) {
//   curl_easy_setopt(curl, CURLOPT_URL, "http://example.com");
 
//   list = curl_slist_append(list, "Shoesize: 10");
//   list = curl_slist_append(list, "Accept:");
 
//   curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);
 
//   curl_easy_perform(curl);
 
//   curl_slist_free_all(list); /* free the list again */
// }

//UPLOAD FILE > 2GB WITH PUT
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

//UPLOAD FILE <= 2GB
// CURL *curl = curl_easy_init();
// if(curl) {
//   long uploadsize = FILE_SIZE;
 
//   curl_easy_setopt(curl, CURLOPT_URL, "ftp://example.com/destination.tar.gz");
 
//   curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
 
//   curl_easy_setopt(curl, CURLOPT_INFILESIZE, uploadsize);
 
//   curl_easy_perform(curl);
// }
