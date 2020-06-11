// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>

#include "filesystem.h"

#include <boost/system/error_code.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <boost/thread/lock_types.hpp>
#include <primitives/exceptions.h>

#ifndef _WIN32
#include <sys/resource.h>
#endif

#define SW_NAME "sw"

namespace sw
{

path get_config_filename()
{
    return get_root_directory() / "sw.yml";
}

path get_root_directory()
{
    return get_home_directory() / "." SW_NAME;
}

path temp_directory_path(const path &subdir)
{
    auto p = fs::temp_directory_path() / SW_NAME / subdir;
    fs::create_directories(p);
    return p;
}

path get_temp_filename(const path &subdir)
{
    return temp_directory_path(subdir) / unique_path();
}

path get_ca_certs_filename()
{
    const path cert_dir = get_root_directory() / "certs";
    path cert_file = cert_dir / "roots.pem";
    return cert_file;
}

String make_archive_name(const String &fn)
{
    if (!fn.empty())
        return fn + ".tar.gz";
    return SW_NAME ".tar.gz";
}

void findRootDirectory1(const path &p, path &root, int depth = 0)
{
    // limit recursion
    if (depth++ > 10)
        return;

    std::vector<path> pfiles;
    std::vector<path> pdirs;
    for (auto &pi : fs::directory_iterator(p))
    {
        auto f = pi.path().filename().string();
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
    /*else if (depth == 1)
    {
        root = p;
    }*/
}

path findRootDirectory(const path &p)
{
    path root;
    findRootDirectory1(p, root);
    return root;
}

void create_directories(const path &p)
{
    static std::unordered_set<path> dirs;
    static boost::upgrade_mutex m;
    boost::upgrade_lock lk(m);
    if (dirs.find(p) != dirs.end())
        return;
    fs::create_directories(p);
    boost::upgrade_to_unique_lock lk2(lk);
    dirs.insert(p);
}

int set_max_open_files_limit(int new_limit)
{
#ifdef _WIN32
    auto old = _getmaxstdio();
    // windows cannot set more than 8192
    if (new_limit > 8192)
        new_limit = 8192;
    if (_setmaxstdio(new_limit) == -1)
        throw SW_RUNTIME_ERROR("Cannot raise number of maximum opened files");
#else
    struct rlimit rlp;
    getrlimit(RLIMIT_NOFILE, &rlp);
    auto old = rlp.rlim_cur;
    rlp.rlim_cur = new_limit;
    if (setrlimit(RLIMIT_NOFILE, &rlp) == -1)
        throw SW_RUNTIME_ERROR("Cannot raise number of maximum opened files");
#endif
    return old;
}

}
