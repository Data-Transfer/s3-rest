#pragma once
#include <algorithm>
#include <map>
#include <string>
#include <vector>

template <class ContainerT>
void split(const std::string& str, ContainerT& cont,
           const std::string& delims = " ",
           const size_t count = std::string::npos) {
    std::size_t cur, prev = 0;
    cur = str.find_first_of(delims);
    size_t i = 0;
    while (cur != std::string::npos && count != i) {
        cont.push_back(str.substr(prev, cur - prev));
        prev = cur + 1;
        cur = str.find_first_of(delims, prev);
        i += 1;
    }
    if (cur == std::string::npos) {
        cont.push_back(str.substr(prev, cur - prev));
    } else {
        cont.push_back(str.substr(prev, std::string::npos));
    }
}

struct URL {
    int port = -1;
    std::string host;
    std::string proto;
};

URL ParseURL(const std::string& s);

std::string UrlEncode(const std::string& s);

std::string UrlEncode(const std::map<std::string, std::string>& p);

struct Time {
    std::string timeStamp;
    std::string dateStamp;
};

Time GetDates();

using Bytes = std::vector<uint8_t>;

Bytes Hash(const Bytes& key, const Bytes& msg);
std::string Hex(const Bytes& b);

Bytes CreateSignatureKey(const std::string& key, const std::string& dateStamp,
                         const std::string& region, const std::string& service);

std::string ToUpper(std::string s);

std::string ToLower(std::string s);

std::map<std::string, std::string> ParseParams(std::string s);
