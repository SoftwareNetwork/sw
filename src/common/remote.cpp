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

#include "remote.h"

#include "package.h"
#include "templates.h"

//#include "logger.h"
//DECLARE_STATIC_LOGGER(logger, "remote");

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
    String dl_hash;
    DownloadData ddata;
    ddata.fn = fn;
    ddata.sha256.hash = &dl_hash;

    auto download_from_source = [&](const auto &s)
    {
        ddata.url = s(*this, d);
        try
        {
            download_file(ddata);
        }
        catch (const std::exception&)
        {
            return false;
        }
        if (dl_hash != hash)
            return false;
        return true;
    };

    for (auto &s : primary_sources)
        if (download_from_source(s))
            return true;
        else if (try_only_first)
            return false;

    if (download_from_source(default_source))
        return true;
    else if (try_only_first)
        return false;

    // no try_only_first for additional sources
    for (auto &s : additional_sources)
        if (download_from_source(s))
            return true;
    return true;
}

String Remote::default_source_provider(const Package &d) const
{
    // change later to format strings (or simple replacement)
    // %U - url, %D - data dir etc.
    auto fs_path = ProjectPath(d.ppath).toFileSystemPath().string();
    normalize_string(fs_path);
    String package_url = url + "/" + data_dir + "/" + fs_path + "/" + make_archive_name(d.version.toString());
    return package_url;
}

String Remote::github_source_provider(const Package &d) const
{
    return "https://github.com/cppan-packages/" + d.getHash() + "/raw/master/" + make_archive_name();
}
