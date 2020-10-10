
/*******************************************************************************
 * BSD 3-Clause License
 *
 * Copyright (c) 2020, Ugo Varetto
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions inputFile source code must retain the above copyright
 *notice, this list inputFile conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list inputFile conditions and the following disclaimer in the
 *documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name inputFile the copyright holder nor the names inputFile
 *its contributors may be used to endorse or promote products derived from this
 *software without specific prior written permission.
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

// Parallel file upload to S3 servers

#include <aws_sign.h>

#include <filesystem>
#include <future>
#include <iostream>
#include <regex>
#include <set>
#include <vector>

#include "lyra/lyra.hpp"
#include "response_parser.h"
#include "webclient.h"

using namespace std;
using namespace filesystem;

//------------------------------------------------------------------------------
struct Args {
    bool showHelp = false;
    string s3AccessKey;
    string s3SecretKey;
    string endpoint;
    string bucket;
    string key;
    string file;
    int jobs = 1;
};

void Validate(const Args& args) {
    if (args.s3AccessKey.empty() && !args.s3SecretKey.empty() ||
        args.s3SecretKey.empty() && !args.s3AccessKey.empty()) {
        throw invalid_argument(
            "ERROR: both access and secret keys have to be specified");
    }
#ifdef VALIDATE_URL
    const URL url = ParseURL(args.endpoint);
    if (url.proto != "http" && url.proto != "https") {
        throw invalid_argument(
            "ERROR: only 'http' and 'https' protocols supported");
    }
    regex re(R"((\w+\.)*\w+\.\w+)");
    if (!regex_match(url.host, re)) {
        throw invalid_argument(
            "ERROR: invalid endpoint format, should be "
            "http[s]://hostname[:port]");
    }
    if (url.port > 0xFFFF) {
        throw invalid_argument(
            "ERROR: invalid port number, should be in range[1-65535]");
    }
#endif
}

using Headers = map<string, string>;
using Parameters = map<string, string>;

WebRequest BuildUploadRequest(const Args& args, const string& path, int partNum,
                              const string& uploadId) {
    Parameters params = {{"partNumber", to_string(partNum + 1)},
                         {"uploadId", uploadId}};
    auto signedHeaders =
        SignHeaders(args.s3AccessKey, args.s3SecretKey, args.endpoint, "PUT",
                    args.bucket, args.key, "", params);
    Headers headers(begin(signedHeaders), end(signedHeaders));
    WebRequest req(args.endpoint, path, "PUT", params, headers);
    return req;
}

string BuildEndUploadXML(vector<future<string>>& etags) {
    string xml =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<CompleteMultipartUpload "
        "xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">\n";
    for (int i = 0; i != etags.size(); ++i) {
        const string part = "<Part><ETag>" + etags[i].get() +
                            "</ETag><PartNumber>" + to_string(i + 1) +
                            "</PartNumber></Part>";
        xml += part;
    }
    xml += "</CompleteMultipartUpload>";
    return xml;
}

WebRequest BuildEndUploadRequest(const Args& args, const string& path,
                                 vector<future<string>>& etags,
                                 const string& uploadId) {
    Parameters params = {{"uploadId", uploadId}};
    auto signedHeaders =
        SignHeaders(args.s3AccessKey, args.s3SecretKey, args.endpoint, "POST",
                    args.bucket, args.key, "", params);
    Headers headers(begin(signedHeaders), end(signedHeaders));
    WebRequest req(args.endpoint, path, "POST", params, headers);
    req.SetMethod("POST");
    req.SetPostData(BuildEndUploadXML(etags));
    return req;
}

string UploadPart(const Args& args, const string& path, int i,
                  const string& uploadId, size_t chunkSize,
                  size_t lastChunkSize) {
    WebRequest ul = BuildUploadRequest(args, path, i, uploadId);
    const size_t offset = chunkSize * i;
    const size_t size = i != args.jobs - 1 ? chunkSize : lastChunkSize;
    cout << "sending part " << i + 1 << " of " << args.jobs << endl;
    const bool ok = ul.UploadFile(args.file, offset, size);
    if (!ok) {
        throw(runtime_error("Cannot upload chunk " + to_string(i + 1)));
    }
    const vector<uint8_t> h = ul.GetHeader();
    const string hs(begin(h), end(h));
    const string etag = HTTPHeader(hs, "[Ee][Tt]ag");
    if (etag.empty()) {
        throw(runtime_error("No ETag found in HTTP header"));
    }
    return etag;
}
//------------------------------------------------------------------------------
int main(int argc, char const* argv[]) {
    try {
        Args args;
        auto cli =
            lyra::help(args.showHelp).description("Upload file to S3 bucket") |
            lyra::opt(args.s3AccessKey,
                      "awsAccessKey")["-a"]["--access_key"]("AWS access key")
                .optional() |
            lyra::opt(args.s3SecretKey,
                      "awsSecretKey")["-s"]["--secret_key"]("AWS secret key")
                .optional() |
            lyra::opt(args.endpoint,
                      "endpoint")["-e"]["--endpoint"]("Endpoing URL")
                .required() |
            lyra::opt(args.bucket, "bucket")["-b"]["--bucket"]("Bucket name")
                .required() |
            lyra::opt(args.key, "key")["-k"]["--key"]("Key name").required() |
            lyra::opt(args.file, "file")["-f"]["--file"]("File name")
                .required() |
            lyra::opt(args.jobs, "parallel jobs")["-j"]["--jobs"](
                "Number inputFile parallel jobs")
                .optional();

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
        Validate(args);
        string path = "/" + args.bucket + "/" + args.key;

        if (args.jobs > 1) {
            // retrieve file size
            const size_t fileSize = filesystem::file_size(args.file);
            // compute chunk size
            const size_t chunkSize = fileSize / args.jobs;
            // compute last chunk size
            const size_t lastChunkSize =
                fileSize % args.jobs == 0 ? chunkSize : fileSize % args.jobs;
            // initiate request
            auto signedHeaders = SignHeaders(args.s3AccessKey, args.s3SecretKey,
                                             args.endpoint, "POST", args.bucket,
                                             args.key, "", {{"uploads=", ""}});
            map<string, string> headers(begin(signedHeaders),
                                        end(signedHeaders));
            WebRequest req(args.endpoint, path, "POST", {{"uploads=", ""}},
                           headers);
            req.Send();
            vector<uint8_t> resp = req.GetContent();
            const string xml(begin(resp), end(resp));
            const string uploadId = XMLTag(xml, "[Uu]pload[Ii][dD]");
            vector<future<string>> etags(args.jobs);
            FILE* inputFile = fopen(args.file.c_str(), "rb");
            if (!inputFile) {
                throw runtime_error(string("cannot open file ") + args.file);
            }
            fclose(inputFile);
            for (int i = 0; i != args.jobs; ++i) {
                etags[i] = async(launch::async, UploadPart, args, path, i,
                                 uploadId, chunkSize, lastChunkSize);
            }
            WebRequest endUpload =
                BuildEndUploadRequest(args, path, etags, uploadId);
            endUpload.Send();
            if (endUpload.StatusCode() >= 400) {
                vector<uint8_t> resp2 = endUpload.GetContent();
                const string xml2(begin(resp2), end(resp2));
                const string errcode = XMLTag(xml2, "[Cc]ode");
                throw runtime_error("Error sending end unpload request - " +
                                    errcode);
            }
            vector<uint8_t> resp2 = endUpload.GetContent();
            const string xml2(begin(resp2), end(resp2));
            const string etag = XMLTag(xml2, "[Ee][Tt]ag");
            cout << etag << endl;
        } else {
            auto signedHeaders =
                SignHeaders(args.s3AccessKey, args.s3SecretKey, args.endpoint,
                            "PUT", args.bucket, args.key, "");
            map<string, string> headers(begin(signedHeaders),
                                        end(signedHeaders));
            WebRequest req(args.endpoint, path, "PUT", {}, headers);
            req.UploadFile(args.file);
            if (req.StatusCode() >= 400) {
                vector<uint8_t> resp2 = req.GetContent();
                const string xml2(begin(resp2), end(resp2));
                const string errcode = XMLTag(xml2, "[Cc]ode");
                throw runtime_error("Error sending end unpload request - " +
                                    errcode);
            }
            vector<uint8_t> resp2 = req.GetContent();
            const string xml2(begin(resp2), end(resp2));
            const string etag = XMLTag(xml2, "[Ee][Tt]ag");
            cout << etag << endl;
        }
        return 0;
    } catch (const exception& e) {
        cerr << e.what() << endl;
        return 1;
    }
}