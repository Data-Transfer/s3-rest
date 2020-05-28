/*******************************************************************************
* BSD 3-Clause License
* 
* Copyright (c) 2020, Ugo Varetto
* All rights reserved.
* 
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
* 
* 1. Redistributions of source code must retain the above copyright notice, this
*    list of conditions and the following disclaimer.
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
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
* SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
* CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*******************************************************************************/
// Declaration and in-line implementation of utility functions to deal with
// URL and strings.
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
