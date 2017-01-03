/*
 * Copyright (C) 2016-2017, Egor Pugin
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <iostream>
#include <regex>
#include <string>

#include <boost/filesystem.hpp>

namespace fs = boost::filesystem;
using path = fs::wpath;

using String = std::string;

String read_file(const path &p)
{
    if (!fs::exists(p))
        throw std::runtime_error("File '" + p.string() + "' does not exist");

    auto fn = p.string();
    std::ifstream ifile(fn, std::ios::in | std::ios::binary);
    if (!ifile)
        throw std::runtime_error("Cannot open file " + fn);

    size_t sz = (size_t)fs::file_size(p);

    String f;
    f.resize(sz);
    ifile.read(&f[0], sz);
    return f;
}

void write_file(const path &p, const String &s)
{
    if (fs::exists(p))
    {
        auto f = read_file(p);
        if (f == s)
            return;
    }

    std::ofstream ofile(p.string(), std::ios::out | std::ios::binary);
    if (!ofile)
        throw std::runtime_error("Cannot open file '" + p.string() + "' for writing");
    ofile << s;
}

String preprocess_file(const String &s)
{
    String o;
    int i = 0;
    for (auto &c : s)
    {
        String h(2, 0);
        sprintf(&h[0], "%02x", c);
        o += "0x" + h + ",";
        if (++i % 25 == 0)
            o += "\n";
        else
            o += " ";
    }
    o += "0x00,";
    if (++i % 25 == 0)
        o += "\n";
    return o;
}

int main(int argc, char *argv[])
{
    if (argc != 3)
        return 1;

    path p = argv[1];
    if (!fs::exists(p))
    {
        std::cerr << "no such file: " << p.string() << "\n";
        return 1;
    }

    auto s = read_file(p);
    std::regex r("CPPAN_INCLUDE<(.*?)>");
    std::smatch m;
    while (std::regex_search(s, m, r))
    {
        String str;
        str = m.prefix();
        path f = m[1].str();
        if (!fs::exists(f))
        {
            std::cerr << "no such file: " << f.string() << "\n";
            return 1;
        }
        str += preprocess_file(read_file(f));
        str += m.suffix();
        s = str;
    }

    write_file(argv[2], s);

    return 0;
}
