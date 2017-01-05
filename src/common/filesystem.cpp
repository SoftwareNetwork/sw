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

#include "filesystem.h"

#include <boost/algorithm/string.hpp>

#include <iostream>
#include <regex>

String get_stamp_filename(const String &prefix)
{
    return prefix + ".sha256";
}

path get_home_directory()
{
#ifdef WIN32
    auto home = getenv("USERPROFILE");
    if (!home)
        std::cerr << "Cannot get user's home directory (%USERPROFILE%)\n";
#else
    auto home = getenv("HOME");
    if (!home)
        std::cerr << "Cannot get user's home directory ($HOME)\n";
#endif
    if (home == nullptr)
        return "";
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

path temp_directory_path(const path &subdir)
{
    auto p = fs::temp_directory_path() / "cppan" / subdir;
    fs::create_directories(p);
    return p;
}

path get_temp_filename(const path &subdir)
{
    return temp_directory_path(subdir) / fs::unique_path();
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
    if (p.empty())
        return "";
    String s = p.string();
    normalize_string(s);
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

std::vector<String> read_lines(const path &p)
{
    auto s = read_file(p);
    return split_lines(s);
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

void copy_dir(const path &src, const path &dst)
{
    fs::create_directories(dst);
    for (auto &f : boost::make_iterator_range(fs::directory_iterator(src), {}))
    {
        if (fs::is_directory(f))
            copy_dir(f, dst / f.path().filename());
        else
            fs::copy_file(f, dst / f.path().filename(), fs::copy_option::overwrite_if_exists);
    }
}

void remove_files_like(const Files &files, const String &regex)
{
    std::regex r(regex);
    for (auto &f : files)
    {
        if (!std::regex_match(f.filename().string(), r))
            continue;
        fs::remove(f);
    }
}

void remove_files_like(const path &dir, const String &regex)
{
    remove_files_like(enumerate_files(dir), regex);
}

Files enumerate_files(const path &dir)
{
    Files files;
    if (!fs::exists(dir))
        return files;
    for (auto &f : boost::make_iterator_range(fs::recursive_directory_iterator(dir), {}))
    {
        if (!fs::is_regular_file(f))
            continue;
        files.insert(f);
    }
    return files;
}

bool is_under_root(path p, const path &root_dir)
{
    if (p.empty())
        return false;
    if (fs::exists(p))
        // Converts p, which must exist, to an absolute path
        // that has no symbolic link, dot, or dot-dot elements.
        p = fs::canonical(p);
    while (!p.empty())
    {
        if (p == root_dir)
            return true;
        p = p.parent_path();
    }
    return false;
}

bool compare_files(const path &fn1, const path &fn2)
{
    // open files at the end
    std::ifstream file1(fn1.string(), std::ifstream::ate | std::ifstream::binary);
    std::ifstream file2(fn2.string(), std::ifstream::ate | std::ifstream::binary);

    // different sizes
    if (file1.tellg() != file2.tellg())
        return false;

    // rewind
    file1.seekg(0);
    file2.seekg(0);

    const int N = 8192;
    char buf1[N], buf2[N];

    while (!file1.eof() && !file2.eof())
    {
        file1.read(buf1, N);
        file2.read(buf2, N);

        auto sz1 = file1.gcount();
        auto sz2 = file2.gcount();

        if (sz1 != sz2)
            return false;

        if (memcmp(buf1, buf2, (size_t)sz1) != 0)
            return false;
    }
    return true;
}

bool compare_dirs(const path &dir1, const path &dir2)
{
    auto traverse_dir = [](const auto &dir)
    {
        std::vector<path> files;
        if (fs::exists(dir))
        for (auto &f : boost::make_iterator_range(fs::recursive_directory_iterator(dir), {}))
        {
            if (!fs::is_regular_file(f))
                continue;
            files.push_back(f);
        }
        return files;
    };

    auto files1 = traverse_dir(dir1);
    auto files2 = traverse_dir(dir2);

    if (files1.empty())
        return false; // throw std::runtime_error("left side has no files");
    if (files2.empty())
        return false; // throw std::runtime_error("right side has no files");
    if (files1.size() != files2.size())
        return false; // throw std::runtime_error("different number of files");

    auto sz = files1.size();
    for (size_t i = 0; i < sz; i++)
    {
        if (!compare_files(files1[i], files2[i]))
            return false;
    }

    return true;
}

void findRootDirectory1(const path &p, path &root, int depth = 0)
{
    // limit recursion
    if (depth++ > 10)
        return;

    std::vector<path> pfiles;
    std::vector<path> pdirs;
    for (auto &pi : boost::make_iterator_range(fs::directory_iterator(p), {}))
    {
        auto f = pi.path().filename().string();
        if (f == CPPAN_FILENAME)
            continue;
        if (fs::is_regular_file(pi))
        {
            pfiles.push_back(pi);
            break;
        }
        else if (fs::is_directory(pi))
        {
            pdirs.push_back(pi);
            if (pdirs.size() > 1)
                break;
        }
    }
    if (pfiles.empty() && pdirs.size() == 1)
    {
        auto d = fs::relative(*pdirs.begin(), p);
        root /= d;
        findRootDirectory1(p / d, root);
    }
}

path findRootDirectory(const path &p)
{
    path root;
    findRootDirectory1(p, root);
    return root;
}
