// Code snippet implementing Python like functions for C++ string manipulation:
// - f-string: substitute '$<N>' with parameter N, options for formatting
//             numbers
// - f-string from dictionary data: substitute keys with values, replace
//   \<key name> with <key value> automatically
// - split string
// - join strings
// - string coversion through ToString function: acts as point of customization;
//   specialize with custom types as needed
// Author: Ugo Varetto
// Copyright (c): Ugo Varetto 2020
// License: zlib i.e. use as you wish, no requirement to mention the author,
//          just do not claim to be the author yourself.

#include <cmath>
#include <iomanip>
#include <iostream>
#include <list>
#include <map>
#include <regex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>

namespace format_string {
std::string ToString(const std::string& s) { return s; }

std::string ToString(const char* s) { return std::string(s); }

template <typename T>
std::string ToString(const T& v) {
    return std::to_string(v);
}

template <typename T, typename... P>
struct FormatInfo {
    const T& ref;
    const std::tuple<P...> pa;
    FormatInfo(const T& r, const std::tuple<P...>& t) : ref(r), pa(t) {}
};

template <typename T, typename... P>
FormatInfo<T, P...> Format(const T& d, const P&... p) {
    return FormatInfo<T, P...>(d, std::tuple<P...>(p...));
}

template <typename T, typename... P>
FormatInfo<T, P...> fm(const T& d, const P&... p) {
    return Format(d, p...);
}

struct IntTypeTag {};
struct FloatTypeTag {};

template <typename T>
struct ToStringSelect {};

template <>
struct ToStringSelect<float> {
    using type = FloatTypeTag;
};

template <>
struct ToStringSelect<double> {
    using type = FloatTypeTag;
};

template <>
struct ToStringSelect<int> {
    using type = IntTypeTag;
};

template <>
struct ToStringSelect<unsigned int> {
    using type = IntTypeTag;
};

template <>
struct ToStringSelect<long> {
    using type = IntTypeTag;
};

template <>
struct ToStringSelect<unsigned long> {
    using type = IntTypeTag;
};

template <>
struct ToStringSelect<long long> {
    using type = IntTypeTag;
};

template <>
struct ToStringSelect<unsigned long long> {
    using type = IntTypeTag;
};

template <typename T, typename... P>
std::string ToStringImpl(FloatTypeTag, const FormatInfo<T, P...>& f) {
    std::ostringstream os;
    const int width = std::get<0>(f.pa) + std::get<1>(f.pa);
    const int precision = std::get<1>(f.pa);
    os << std::fixed << std::setw(width) << std::setprecision(precision);
    os << f.ref;
    return os.str();
}

enum { Dec, Hex, Oct } DecimalTypes;

template <typename T, typename... P>
std::string ToStringImpl(IntTypeTag, const FormatInfo<T, P...>& f) {
    std::ostringstream os;
    const int width = std::get<1>(f.pa);
    const char filler = std::get<0>(f.pa);
    auto base = std::dec;
    if (std::get<2>(f.pa) == Hex)
        base = std::hex;
    else if (std::get<2>(f.pa) == Oct)
        base = std::oct;
    os << std::setfill(filler) << std::setw(width) << base << f.ref;
    return os.str();
}

template <typename T, typename... P>
std::string ToString(const FormatInfo<T, P...>& f) {
    return ToStringImpl(typename ToStringSelect<T>::type(), f);
}

namespace detail {
void F_Helper(std::string&, int) {}

template <typename Head, typename... Tail>
void F_Helper(std::string& s, int count, const Head& h, const Tail&... tail) {
    std::regex re("\\$" + std::to_string(count));
    s = std::regex_replace(s, re, ToString(h));
    F_Helper(s, count + 1, tail...);
}

}  // namespace detail

template <typename... ArgsT>
std::string Format(std::string s, const ArgsT&... args) {
    detail::F_Helper(s, 0, args...);
    return std::regex_replace(s, std::regex("\\\\\\$"), "$");
}

template <typename KeyT, typename ValueT>
std::string Format(std::string s, const std::map<KeyT, ValueT>& d) {
    for (auto kv : d) {
        std::regex re(kv.first);
        s = std::regex_replace(s, re, ToString(kv.second));
    }
    return s;
}

template <typename... ArgsT>
std::string f(std::string s, const ArgsT&... args) {
    return Format(s, args...);
}

template <typename KeyT, typename ValueT>
std::string f(std::string s, const std::map<KeyT, ValueT>& d) {
    return Format(s, d);
}

template <typename KeyT, typename ValueT>
std::string k(std::string s, const std::map<KeyT, ValueT>& d,
              const std::string varDelim = "\\\\") {
    for (auto kv : d) {
        std::regex re(varDelim + kv.first);
        s = std::regex_replace(s, re, ToString(kv.second));
    }
    return s;
}

template <typename KeyT, typename ValueT>
std::string k(std::string s, const std::unordered_map<KeyT, ValueT>& d,
              const std::string varDelim = "\\\\") {
    for (auto kv : d) {
        std::regex re(varDelim + kv.first);
        s = std::regex_replace(s, re, ToString(kv.second));
    }
    return s;
}

template <typename ItT>
std::string join(const std::string& s, ItT begin, ItT end) {
    std::string ret = "";
    ItT last = --end;
    while (begin != last) {
        ret += *begin++ + s;
    }
    ret += *last;
    return ret;
}

template <typename CollectionT>
std::string join(const std::string& s, const CollectionT& collection) {
    return join(s, begin(collection), end(collection));
}

template <typename CollectionT>
void split(const std::string& s, const std::string& sepRegEx,
           CollectionT& splits, const char replacedSep = '\n') {
    std::string repsep;
    repsep.push_back(replacedSep);
    const std::string replaced =
        std::regex_replace(s, std::regex(sepRegEx), repsep);
    std::istringstream is(replaced);
    std::string token;
    while (std::getline(is, token, replacedSep)) {
        if (!token.empty()) {
            splits.push_back(token);
        }
    }
}

}  // namespace format_string

using namespace std;
int main(int argc, char const* argv[]) {
    using namespace format_string;
    
    const string s = f(
        R"($0: \$$1, $2: \$$3 another $0, $0/$2=$4 $5 hex = $6)", "car", 19000,
        "truck", 130000, fm(2000.41234f, 1, 2), 124, fm(124, '0', 3, Hex));
    cout << s << endl;
    const map<string, string> h = {{"Content-length", "2048"},
                                   {"Content-type", "application/json"}};
    cout << k("Content-length: \\Content-length\nContent-type: \\Content-type",
              h)
         << endl;
    cout << k("Content-length: $Content-length\nContent-type: $Content-type", h,
              "\\$")  // regex delimiter, need to use \$ for '$' since it's a
                      // special regex parameter
         << endl;

    const string params = "key1=val1;key2=val2;key3=val3";
    std::list<std::string> tokens;
    split(params, string(";"), tokens);
    for (auto t : tokens) {
        cout << t << " ";
    }
    cout << endl << join(";", begin(tokens), end(tokens)) << endl;
    return 0;
}
