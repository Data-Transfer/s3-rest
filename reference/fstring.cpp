#include <cmath>
#include <iostream>
#include <regex>
#include <string>
#include <utility>
#include <sstream>
#include <iomanip>
#include <type_traits>

namespace format_string {
std::string ToString(const std::string& s) { return s; }

std::string ToString(const char* s) { return std::string(s); }

template <typename T>
std::string ToString(const T& v) {
    return std::to_string(v);
}

template <typename T, typename...P>
struct FormatInfo {
    const T& ref;
    const std::tuple<P...> pa;
    FormatInfo(const T& r, const std::tuple<P...>& t) : ref(r), pa(t) {} 
};

template < typename T, typename...P> 
FormatInfo<T, P...> Format(const T& d, const P&...p) {
    return FormatInfo<T, P...>(d, std::tuple<P...>(p...));
}

template < typename T, typename...P> 
FormatInfo<T, P...> fm(const T& d, const P&...p) {
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

template <typename T, typename...P>
std::string ToStringImpl(FloatTypeTag, const FormatInfo<T, P...>& f) {
    std::ostringstream os;
    const int width = std::get<0>(f.pa) + std::get<1>(f.pa);
    const int precision = std::get<1>(f.pa);
    os << std::fixed  << std::setw(width) <<  std::setprecision(precision);
    os << f.ref;
    return os.str();
}

enum {dec, hex, oct} DecimalTypes;

template <typename T, typename...P>
std::string ToStringImpl(IntTypeTag, const FormatInfo<T, P...>& f) {
    std::ostringstream os;
    const int width = std::get<1>(f.pa);
    const char filler = std::get<0>(f.pa);
    auto base = std::dec;
    if(std::get<2>(f.pa) == hex) base = std::hex;
    else if(std::get<2>(f.pa) == oct) base = std::oct;
    os << std::setfill(filler) << std::setw(width) << base << f.ref;
    return os.str();
}

template <typename T, typename...P>
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


}  // namespace deatail
template <typename T>
T Round(const T& f, int digits) {
    const T F = pow(10, digits);
    return int64_t(f * F) / F;
}

template <typename... ArgsT>
std::string Format(std::string s, const ArgsT&... args) {
    detail::F_Helper(s, 0, args...);
    return std::regex_replace(s, std::regex("\\\\\\$"), "$");
}

template <typename... ArgsT>
std::string f(std::string s, const ArgsT&... args) {
    return Format(s, args...);
}

}  // namespace format_string

int main(int argc, char const* argv[]) {
    using namespace format_string;
    const std::string s =
        f("$0: \\$$1, $2: \\$$3 another $0, $0/$2=$4 $5", 
              "car", 19000, "truck", 130000, fm(2000.41234f, 1, 2), fm(124, '0', 3, hex));
    std::cout << s << std::endl;
    return 0;
}
