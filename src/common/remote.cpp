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
