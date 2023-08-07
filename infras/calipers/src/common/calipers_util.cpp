/**
 * Copyright (c) Microsoft Corporation.
 * 
 * MIT License
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED *AS IS*, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
/* Author: baichen.bai@alibaba-inc.com */


#include <iostream>
#include <boost/filesystem.hpp>
#include <vector>
#include <algorithm>

#include "calipers_util.h"
#include "calipers_types.h"

using namespace std;

vector<string> split_string(string str, char c)
{
    vector<string> v;
    size_t pos = 0;

    while (true)
    {
        size_t char_pos = str.find(c, pos);
        v.push_back(str.substr(pos, char_pos - pos));

        if (char_pos == string::npos)
        {
            break;
        }

        pos = char_pos + 1;
    }

    return v;
}

uint64_t unsigned_diff(uint64_t a, uint64_t b)
{
    if (a > b)
    {
        return a - b;
    }
    else
    {
        return b - a;
    }
}

void print_instruction(Instruction& instr)
{
}

string& lstrip(string& s) {
    auto iter = find_if(s.begin(), s.end(), [](char c) {
            return !std::isspace<char>(c, std::locale::classic());
        }
    );
    s.erase(s.begin(), iter);
    return s;
}

string& rstrip(string& s) {
    auto iter = find_if(s.rbegin(), s.rend(), [](char c) {
            return !std::isspace<char>(c, std::locale::classic());
        }
    );
    s.erase(iter.base(), s.end());
    return s;
}


string get_base_name(const string& path) {
    return boost::filesystem::path(path).stem().string();
}


string get_linux_base_name(const string& path) {
	string::size_type pos = path.find_last_of('/') + 1;
	string filename = path.substr(pos, path.length() - pos);
	return filename;
}


string get_benchmark_name(const string& path) {
	// a dedicated function
	string::size_type end = path.find_last_of('/');
	string::size_type start = path.find_last_of('/', end - 1);
	return path.substr(start + 1, end - start - 1);
}


string str_to_lower(string s) {
    string s_lower;
    transform(s.begin(), s.end(), s_lower.begin(), [](unsigned char c) {
            return tolower(c);
        }
    );
    return s_lower;
}


bool starts_with(const std::string& s, const std::string& p) noexcept {
    return boost::starts_with(s, p);
}
