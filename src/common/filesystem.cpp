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

#include "cppan_string.h"

#include <boost/algorithm/string.hpp>

#ifdef WIN32
#include <libarchive/archive.h>
#include <libarchive/archive_entry.h>
#else
#include <archive.h>
#include <archive_entry.h>
#endif

#include <iostream>
#include <regex>

#if !defined(_WIN32) && !defined(__APPLE__)
#include <linux/limits.h>
#endif

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
    fs::create_directory(p);
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

bool pack_files(const path &fn, const Files &files, const path &root_dir)
{
    bool result = true;
    auto a = archive_write_new();
    archive_write_add_filter_gzip(a);
    archive_write_set_format_pax_restricted(a);
    archive_write_open_filename(a, fn.string().c_str());
    for (auto &f : files)
    {
        if (!fs::exists(f))
        {
            result = false;
            continue;
        }

        // skip symlinks too
        if (!fs::is_regular_file(f))
            continue;

        auto sz = fs::file_size(f);
        auto e = archive_entry_new();
        archive_entry_set_pathname(e, fs::relative(f, root_dir).string().c_str());
        archive_entry_set_size(e, sz);
        archive_entry_set_filetype(e, AE_IFREG);
        archive_entry_set_perm(e, 0644);
        archive_write_header(a, e);
        auto fp = fopen(f.string().c_str(), "rb");
        if (!fp)
        {
            archive_entry_free(e);
            result = false;
            continue;
        }
        char buff[8192];
        while (auto len = fread(buff, 1, sizeof(buff), fp))
            archive_write_data(a, buff, len);
        fclose(fp);
        archive_entry_free(e);
    }
    archive_write_close(a);
    archive_write_free(a);
    return result;
}

Files unpack_file(const path &fn, const path &dst)
{
    if (!fs::exists(dst))
        fs::create_directories(dst);

    Files files;

    auto a = archive_read_new();
    archive_read_support_filter_all(a);
    archive_read_support_format_all(a);
    auto r = archive_read_open_filename(a, fn.string().c_str(), 10240);
    if (r != ARCHIVE_OK)
        throw std::runtime_error(archive_error_string(a));
    archive_entry *entry;
    while (archive_read_next_header(a, &entry) == ARCHIVE_OK)
    {
        // do not act on symlinks
        auto type = archive_entry_filetype(entry);
        if (type == AE_IFLNK || type != AE_IFREG)
            continue;

        path f = dst / archive_entry_pathname(entry);
        path fdir = f.parent_path();
        if (!fs::exists(fdir))
            fs::create_directories(fdir);
        path filename = f.filename();
        if (filename == "." || filename == "..")
            continue;
        auto fn = fs::absolute(f).string();
        std::ofstream o(fn, std::ios::out | std::ios::binary);
        if (!o)
        {
            // TODO: probably remove this and linux/limit.h header when server will be using hash paths
#ifdef _WIN32
            if (fn.size() >= MAX_PATH)
                continue;
#elif defined(__APPLE__)
#else
            if (fn.size() >= PATH_MAX)
                continue;
#endif
            throw std::runtime_error("Cannot open file: " + f.string());
        }
        for (;;)
        {
            const void *buff;
            size_t size;
            int64_t offset;
            auto r = archive_read_data_block(a, &buff, &size, &offset);
            if (r == ARCHIVE_EOF)
                break;
            if (r < ARCHIVE_OK)
                throw std::runtime_error(archive_error_string(a));
            o.write((const char *)buff, size);
        }
        files.insert(f);
    }
    archive_read_close(a);
    archive_read_free(a);

    return files;
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
