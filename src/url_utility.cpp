#include <cassert>
#include <iomanip>
#include <map>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include "url_utility.h"
using namespace std;

//------------------------------------------------------------------------------
// Break url into {protocol, hostname, port}
URL ParseURL(const string& s) {
    smatch m;
    // minimal version of a URL parser: no enforcement of having the host
    // section start and end with a word and requiring a '.' character
    // separating the individual words.
    regex e("\\s*(\\w+)://([a-zA-Z-_\\.])(:(\\d+))?");
    regex_search(s, m, e, regex_constants::match_any);
    if (m.size() == 3) {
        return {80, m[2], m[1]};
    } else if (m.size() == 5) {
        return {stoi(m[4]), m[2], m[1]};
    }
    return {};
}

//------------------------------------------------------------------------------
// urlencode string
string UrlEncode(const std::string& s) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (std::string::const_iterator i = s.begin(), n = s.end(); i != n; ++i) {
        std::string::value_type c = (*i);

        // Keep alphanumeric and other accepted characters intact
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
            continue;
        }

        // Any other characters are percent-encoded
        escaped << std::uppercase;
        escaped << '%' << setw(2) << int((unsigned char)c);
        escaped << std::nouppercase;
    }

    return escaped.str();
}

//------------------------------------------------------------------------------
// Return urlencoded url request parameters from {key, value} dictionary
string UrlEncode(const map<string, string>& p) {
    string url;
    for (auto i : p) {
        url += UrlEncode(i.first) + "=" + UrlEncode(i.second);
        url += '&';
    }
    return string(url.begin(), --url.end());
}

//------------------------------------------------------------------------------
// Uppercase
string ToUpper(string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::toupper(c); });
    return s;
}

//------------------------------------------------------------------------------
// Lowercase
string ToLower(string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return s;
}

//------------------------------------------------------------------------------
// From "key1=value1;key2=value2;key3=;key4" to {key, value} dictionary
map<string, string> ParseParams(string s) {
    if (s.empty()) return map<string, string>();
    if (s.back() != '=') s += '=';
    vector<string> slist;
    split(s, slist, ";");
    map<string, string> params;
    for (auto p : slist) {
        vector<string> kv;
        split(p, kv, "=", 1);
        assert(kv.size() == 1 || kv.size() == 2);
        const string key = kv[0];
        const string value = kv.size() == 2 ? kv[1] : "";
        params.insert({key, value});
    }
    return params;
}