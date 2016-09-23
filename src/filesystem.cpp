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

#include "filesystem.h"

#include "common.h"

#include <boost/algorithm/string.hpp>

#include <iostream>
#include <regex>

String get_stamp_filename(const String &prefix)
{
    return prefix + ".md5";
}

path get_home_directory()
{
#ifdef WIN32
    auto home = getenv("USERPROFILE");
    if (!home)
        throw std::runtime_error("Cannot get user's home directory (%USERPROFILE%)");
#else
    auto home = getenv("HOME");
    if (!home)
        throw std::runtime_error("Cannot get user's home directory ($HOME)");
#endif
    return home;
}

path get_config_filename()
{
    return get_root_directory() / CPPAN_FILENAME;
}

path get_root_directory()
{
    return get_home_directory() / ".cppan";
}

String make_archive_name(const String &fn)
{
    if (!fn.empty())
        return fn + ".tar.gz";
    return "cppan.tar.gz";
}

path temp_directory_path()
{
    auto p = fs::temp_directory_path() / "cppan";
    fs::create_directory(p);
    return p;
}

path get_temp_filename()
{
    return temp_directory_path() / fs::unique_path();
}

path temp_script_path()
{
    auto p = temp_directory_path() / "scripts";
    fs::create_directory(p);
    return p;
}

path temp_script_filename()
{
    return temp_script_path() / fs::unique_path();
}

void remove_file(const path &p)
{
    boost::system::error_code ec;
    fs::remove(p, ec);
    if (ec)
        std::cerr << "Cannot remove file: " << p.string() << "\n";
}

String normalize_path(const path &p)
{
    String s = p.string();
    boost::algorithm::replace_all(s, "\\", "/");
    return s;
}

String read_file(const path &p, bool no_size_check)
{
    if (!fs::exists(p))
        throw std::runtime_error("File '" + p.string() + "' does not exist");

    auto fn = p.string();
    std::ifstream ifile(fn, std::ios::in | std::ios::binary);
    if (!ifile)
        throw std::runtime_error("Cannot open file '" + fn + "' for reading");

    size_t sz = (size_t)fs::file_size(p);
    if (!no_size_check && sz > 10'000'000)
        throw std::runtime_error("File " + fn + " is very big (> ~10 MB)");

    String f;
    f.resize(sz);
    ifile.read(&f[0], sz);
    return f;
}

void write_file(const path &p, const String &s)
{
    auto pp = p.parent_path();
    if (!pp.empty())
        fs::create_directories(pp);

    std::ofstream ofile(p.string(), std::ios::out | std::ios::binary);
    if (!ofile)
        throw std::runtime_error("Cannot open file '" + p.string() + "' for writing");
    ofile << s;
}

void write_file_if_different(const path &p, const String &s)
{
    if (fs::exists(p))
    {
        auto s2 = read_file(p);
        if (s == s2)
            return;
    }

    fs::create_directories(p.parent_path());

    std::ofstream ofile(p.string(), std::ios::out | std::ios::binary);
    if (!ofile)
        throw std::runtime_error("Cannot open file '" + p.string() + "' for writing");
    ofile << s;
}

void copy_dir(const path &source, const path &destination)
{
    fs::create_directories(destination);
    for (auto &f : boost::make_iterator_range(fs::directory_iterator(source), {}))
    {
        if (fs::is_directory(f))
            copy_dir(f, destination / f.path().filename());
        else
            fs::copy_file(f, destination / f.path().filename(), fs::copy_option::overwrite_if_exists);
    }
}

void remove_files_like(const path &dir, const String &regex)
{
    if (!fs::exists(dir))
        return;
    std::regex r(regex);
    for (auto &f : boost::make_iterator_range(fs::recursive_directory_iterator(dir), {}))
    {
        if (!fs::is_regular_file(f))
            continue;
        if (!std::regex_match(f.path().filename().string(), r))
            continue;
        fs::remove(f);
    }
}

bool is_under_root(path p, const path &root_dir)
{
    if (!p.empty() && fs::exists(p))
        p = fs::canonical(p);
    while (!p.empty())
    {
        if (p == root_dir)
            return true;
        p = p.parent_path();
    }
    return false;
}
