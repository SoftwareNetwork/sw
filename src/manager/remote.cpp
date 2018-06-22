// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "remote.h"

#include "package.h"

#include <hash.h>
#include <http.h>
#include <primitives/templates.h>

//#include "logger.h"
//DECLARE_STATIC_LOGGER(logger, "remote");

namespace sw
{

Remotes get_default_remotes()
{
    static Remotes rms;
    RUN_ONCE
    {
        Remote r;
        r.name = DEFAULT_REMOTE_NAME;
        r.url = "https://cppan.org/";
        r.data_dir = "data";
        r.primary_sources.push_back(&Remote::github_source_provider);
        rms.push_back(r);
    };
    return rms;
}

bool Remote::downloadPackage(const Package &d, const String &hash, const path &fn, bool try_only_first) const
{
    auto download_from_source = [&](const auto &s)
    {
        try
        {
            download_file(s(*this, d), fn);
        }
        catch (const std::exception&)
        {
            return false;
        }
        return check_strong_file_hash(fn, hash) || check_file_hash(fn, hash);
    };

    for (auto &s : primary_sources)
    {
        if (download_from_source(s))
            return true;
        else if (try_only_first)
            return false;
    }

    if (download_from_source(default_source))
        return true;
    else if (try_only_first)
        return false;

    // no try_only_first for additional sources
    for (auto &s : additional_sources)
    {
        if (download_from_source(s))
            return true;
    }
    return false;
}

String Remote::default_source_provider(const Package &d) const
{
    // TODO: change later to format strings (or simple replacement)
    // %U - url, %D - data dir etc.
    auto fs_path = PackagePath(d.ppath).toFileSystemPath().string();
    normalize_string(fs_path);
    String package_url = url + "/" + data_dir + "/" + fs_path + "/" + make_archive_name(d.version.toString());
    return package_url;
}

String Remote::cppan2_source_provider(const Package &d) const
{
    // TODO: change later to format strings (or simple replacement)
    // %U - url, %D - data dir etc.
    auto fs_path = d.getHashPathFull().string();
    normalize_string(fs_path);
    String package_url = url;
    if (package_url.back() != '/')
        package_url += "/";
    package_url += data_dir + "/" + fs_path + "/" + make_archive_name("sw");
    return package_url;
}

String Remote::github_source_provider(const Package &d) const
{
    return "https://github.com/cppan-packages/" + d.getHash() + "/raw/master/" + make_archive_name();
}

}
