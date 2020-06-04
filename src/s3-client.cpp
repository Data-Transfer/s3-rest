
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

// Send S3v4 signed REST requests

#include <iostream>

#include <aws_sign.h>

#include "lyra/lyra.hpp"
#include "webclient.h"

using namespace std;



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
