#include <cmath>
#include <iostream>
#include <regex>
#include <string>

namespace format_string {
std::string ToString(const std::string& s) { return s; }

std::string ToString(const char* s) { return std::string(s); }

template <typename T>
std::string ToString(const T& v) {
    return std::to_string(v);
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
std::string F(std::string s, const ArgsT&... args) {
    detail::F_Helper(s, 0, args...);
    return std::regex_replace(s, std::regex("\\\\\\$"), "$");
}
}  // namespace format_string

int main(int argc, char const* argv[]) {
    namespace fs = format_string;
    const std::string s =
        fs::F("$0: \\$$1, $2: \\$$3 another $0, $0/$2=$4", 
              "car", 19000, "truck", 130000, fs::Round(float(19)/130, 2));
    std::cout << s << std::endl;
    return 0;
}
