
#include <iostream>
#include <string>
#include <map>

#include "aws_sign.h"
#include "url_utility.h"
#include "lyra/lyra.hpp"

using namespace std;

//------------------------------------------------------------------------------
struct Args {
    bool showHelp = false;
    string awsAccessKey;
    string awsSecretKey;
    string endpoint;
    string bucket;
    string key;
    string params;
    string method;
    int expiration = 3600;
};

//------------------------------------------------------------------------------
void PrintArgs(const Args& args) {
    cout << "awsAccessKey: " << args.awsAccessKey << endl
         << "awsSecretKey: " << args.awsSecretKey << endl
         << "endpoint:     " << args.endpoint << endl
         << "method:       " << ToUpper(args.method) << endl
         << "bucket:       " << args.bucket << endl
         << "key:          " << args.key << endl
         << "expiration:   " << args.expiration << endl
         << "parameters:   " << args.params << endl;
}

//------------------------------------------------------------------------------
// Generate and sign url for usage with AWS S3 compatible environment such
// as Amazone services and Ceph object gateway
int main(int argc, char const* argv[]) {
    // The parser with the multiple option arguments and help option.
    Args args;
    auto cli =
        lyra::help(args.showHelp).description("Pre-sign S3 URLs") |
        lyra::opt(args.awsAccessKey,
                  "awsAccessKey")["-a"]["--access_key"]("AWS access key")
            .required() |
        lyra::opt(args.awsSecretKey,
                  "awsSecretKey")["-s"]["--secret_key"]("AWS secret key")
            .required() |
        lyra::opt(args.endpoint, "endpoint")["-e"]["--endpoint"]("Endpoing URL")
            .required() |
        lyra::opt(args.method, "method")["-m"]["--method"](
            "HTTP method: get | put | post | delete")
            .required() |
        lyra::opt(args.params, "params")["-p"]["--params"](
            "URL request parameters. key1=value1;key2=...")
            .optional() |
        lyra::opt(args.bucket, "bucket")["-b"]["--bucket"]("Bucket name") |
        lyra::opt(args.expiration, "expiration")["-t"]["--expiration"](
            "expiration time in seconds")
            .optional() |
        lyra::opt(args.key, "key")["-k"]["--key"]("Key name").optional();

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
    // PrintArgs(args);
    const map<string, string> params = ParseParams(args.params); 
    const string signedURL =
        SignedURL(args.awsAccessKey, args.awsSecretKey, args.expiration,
                  args.endpoint, ToUpper(args.method), args.bucket, args.key);
    cout << signedURL;
    return 0;
}