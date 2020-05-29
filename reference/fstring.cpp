#include <iostream>
#include <string>
#include <regex>
#include <cmath>

using namespace std;


string ToString(const string& s) {
    return s;
}

string ToString(const char* s) {
    return string(s);
}

template <typename T>
string ToString(const T& v) {
    return to_string(v);
}


void F_Helper(string&, int) {}

template <typename Head, typename...Tail>
void F_Helper(string& s, int count, const Head& h, const Tail&...tail) {
    regex re("\\$" + to_string(count));
    s = regex_replace(s, re, ToString(h));
    F_Helper(s, count+1, tail...);
}

template <typename T>
T Round(const T& f, int digits) {
    const T F = pow(10, digits);
    return int64_t(f * F)/F;
}

template <typename...ArgsT>
string F(string s, const ArgsT&...args) {
    F_Helper(s, 0, args...);
    return regex_replace(s, regex("\\\\\\$"), "$");
}

int main(int argc, char const *argv[]) {
    
    const string s = F("$0: \\$$1, $2: \\$$3 another $0", "car", 
                       19000, "truck", 130000);
    cout << s << endl;
    return 0;
}
