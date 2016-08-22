/*
 * Copyright (c) 2016, Egor Pugin
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     1. Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *     2. Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the distribution.
 *     3. Neither the name of the copyright holder nor the names of
 *        its contributors may be used to endorse or promote products
 *        derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
        str += read_file(f);
        str += m.suffix();
        s = str;
    }

    write_file(argv[2], s);

    return 0;
}
