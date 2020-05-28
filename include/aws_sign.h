
#pragma once
#include <map>
#include <string>


std::string SignedURL(const std::string& accessKey,
                      const std::string& secretKey, int expiration,
                      const std::string& endpoint, const std::string& method,
                      const std::string& bucketName = "",
                      const std::string& keyName = "",
                      const std::map<std::string, std::string>& params =
                          std::map<std::string, std::string>(),
                      const std::string& region = "us-east-1");