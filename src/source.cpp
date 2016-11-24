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

#include "source.h"

#include "http.h"
#include "templates.h"

bool load_source(const yaml &root, Source &source)
{
    auto &src = root["source"];
    if (!src.IsDefined())
        return false;

    auto error = "Only one source must be specified";

    Git git;
    EXTRACT_VAR(src, git.url, "git", String);
    EXTRACT_VAR(src, git.branch, "branch", String);
    EXTRACT_VAR(src, git.tag, "tag", String);

    if (!git.url.empty())
    {
        if (src["file"].IsDefined())
            throw std::runtime_error(error);
        source = git;
    }
    else
    {
        RemoteFile rf;
        EXTRACT_VAR(src, rf.url, "remote", String);

        if (!rf.url.empty())
            source = rf;
        else
            throw std::runtime_error(error);
    }

    return true;
}

void save_source(yaml &root, const Source &source)
{
    auto save_source = overload(
        [&root](const Git &git)
    {
        root["source"]["git"] = git.url;
        if (!git.tag.empty())
            root["source"]["tag"] = git.tag;
        if (!git.branch.empty())
            root["source"]["branch"] = git.branch;
    },
        [&root](const RemoteFile &rf)
    {
        root["source"]["remote"] = rf.url;
    }
    );

    boost::apply_visitor(save_source, source);
}

void DownloadSource::operator()(const Git &git)
{
    // try to speed up git downloads from github
    // add more sites below
    if (git.url.find("github.com") != git.url.npos)
    {
        auto url = git.url;

        // remove possible .git suffix
        String suffix = ".git";
        if (url.rfind(suffix) == url.size() - suffix.size())
            url.substr(0, url.size() - suffix.size());

        String fn;
        url += "/archive/";
        if (!git.tag.empty())
        {
            url += git.tag + ".tar.gz";
            fn = "1.tar.gz";
        }
        else if (!git.branch.empty())
        {
            url += git.branch + ".zip"; // but use .zip for branches!
            fn = "1.zip";
        }

        try
        {
            DownloadData dd;
            dd.url = url;
            dd.fn = fn;
            dd.file_size_limit = max_file_size;
            ::download_file(dd);

            unpack_file(fn, ".");
            fs::remove(fn);

            std::set<path> dirs;
            for (auto &f : boost::make_iterator_range(fs::directory_iterator(fs::current_path()), {}))
            {
                if (!fs::is_directory(f))
                    continue;
                dirs.insert(f);
            }
            if (dirs.size() == 1)
            {
                fs::current_path(*dirs.begin());
                root_dir = *dirs.begin();
            }

            return;
        }
        catch (...)
        {
            // go to usual git download
        }
    }

    // usual git download via clone
#ifdef CPPAN_TEST
    if (fs::exists(".git"))
        return;
#endif

    auto run = [](const String &c)
    {
        if (std::system(c.c_str()) != 0)
            throw std::runtime_error("Last command failed: " + c);
    };

    int n_tries = 3;
    while (n_tries--)
    {
        try
        {
            run("git init");
            run("git remote add origin " + git.url);
            if (!git.tag.empty())
                run("git fetch --depth 1 origin refs/tags/" + git.tag);
            else if (!git.branch.empty())
                run("git fetch --depth 1 origin " + git.branch);
            run("git reset --hard FETCH_HEAD");
            break;
        }
        catch (...)
        {
            if (n_tries == 0)
                throw;
        }
    }
}

void DownloadSource::operator()(const RemoteFile &rf)
{
    auto fn = path(rf.url).filename();

    DownloadData dd;
    dd.url = rf.url;
    dd.fn = fn;
    dd.file_size_limit = max_file_size;
    ::download_file(dd);

    unpack_file(fn, ".");
    fs::remove(fn);
}

void DownloadSource::download(const Source &source)
{
    boost::apply_visitor(*this, source);
}
