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

#include "source.h"

#include "http.h"

#include <primitives/overloads.h>
#include <primitives/pack.h>

bool Git::isValid(String *error) const
{
    if (empty())
    {
        if (error)
            *error = "Git url is missing";
        return false;
    }

    int e = 0;
    e += !tag.empty();
    e += !branch.empty();
    e += !commit.empty();

    if (e == 0)
    {
        if (error)
            *error = "No git sources (tag or branch or commit) available";
        return false;
    }

    if (e > 1)
    {
        if (error)
            *error = "Only one git source (tag or branch or commit) must be specified";
        return false;
    }

    return true;
}

bool load_source(const yaml &root, Source &source)
{
    auto &src = root["source"];
    if (!src.IsDefined())
        return false;

    auto error = "Only one source must be specified";

    Git git;
    YAML_EXTRACT_VAR(src, git.url, "git", String);
    YAML_EXTRACT_VAR(src, git.tag, "tag", String);
    YAML_EXTRACT_VAR(src, git.branch, "branch", String);
    YAML_EXTRACT_VAR(src, git.commit, "commit", String);

    if (!git.url.empty())
    {
        if (src["file"].IsDefined())
            throw std::runtime_error(error);
        source = git;
    }
    else if (src["remote"].IsDefined())
    {
        RemoteFile rf;
        YAML_EXTRACT_VAR(src, rf.url, "remote", String);

        if (!rf.url.empty())
            source = rf;
        else
            throw std::runtime_error(error);
    }
    else if (src["files"].IsDefined())
    {
        RemoteFiles rfs;
        rfs.urls = get_sequence_set<String>(src, "files");
        if (rfs.urls.empty())
            throw std::runtime_error("Empty remote files");
        source = rfs;
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
        if (!git.commit.empty())
            root["source"]["commit"] = git.commit;
    },
        [&root](const RemoteFile &rf)
    {
        root["source"]["remote"] = rf.url;
    },
        [&root](const RemoteFiles &rfs)
    {
        for (auto &rf : rfs.urls)
            root["source"]["files"].push_back(rf);
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
            url += make_archive_name(git.tag);
            fn = make_archive_name("1");
        }
        else if (!git.branch.empty())
        {
            url += git.branch + ".zip"; // but use .zip for branches!
            fn = "1.zip";
        }
        else if (!git.commit.empty())
        {
            url += git.commit + ".zip"; // but use .zip for branches!
            fn = "1.zip";
        }

        try
        {
            download_and_unpack(url, fn);
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
            else if (!git.commit.empty())
            {
                run("git fetch");
                run("git checkout " + git.commit);
            }
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
    download_and_unpack(rf.url, path(rf.url).filename());
}

void DownloadSource::operator()(const RemoteFiles &rfs)
{
    for (auto &rf : rfs.urls)
        download_file(rf, path(rf).filename());
}

void DownloadSource::download_file(const String &url, const path &fn)
{
    checkSourceUrl(url);
    ::download_file(url, fn, max_file_size);
}

void DownloadSource::download_and_unpack(const String &url, const path &fn)
{
    download_file(url, fn);
    unpack_file(fn, ".");
    fs::remove(fn);
}

void DownloadSource::download(const Source &source)
{
    boost::apply_visitor(*this, source);
}

bool isValidSourceUrl(const Source &source)
{
    auto check_url = overload(
        [](const Git &git)
    {
        if (!isValidSourceUrl(git.url))
            return false;
        return true;
    },
        [](const RemoteFile &rf)
    {
        if (!isValidSourceUrl(rf.url))
            return false;
        return true;
    },
        [](const RemoteFiles &rfs)
    {
        for (auto &rf : rfs.urls)
            if (!isValidSourceUrl(rf))
                return false;
        return true;
    }
    );
    return boost::apply_visitor(check_url, source);
}

Source load_source(const ptree &p)
{
    {
        Git git;
        git.url = p.get("source.git.url", "");
        git.tag = p.get("source.git.tag", "");
        git.branch = p.get("source.git.branch", "");
        git.commit = p.get("source.git.commit", "");
        if (!git.empty())
            return git;
    }

    {
        RemoteFile rf;
        rf.url = p.get("source.remote.url", "");
        if (!rf.url.empty())
            return rf;
    }

    {
        RemoteFiles rfs;
        auto urls = p.get_child("source.files");
        for (auto &url : urls)
            rfs.urls.insert(url.second.get("url", ""s));
        if (!rfs.urls.empty())
            return rfs;
    }

    throw std::runtime_error("Bad source");
}

void save_source(ptree &p, const Source &source)
{
    auto write_json = overload(
        [&p](const Git &git)
    {
        if (git.empty())
            return;
        p.add("source.git.url", git.url);
        if (!git.tag.empty())
            p.add("source.git.tag", git.tag);
        if (!git.branch.empty())
            p.add("source.git.branch", git.branch);
        if (!git.commit.empty())
            p.add("source.git.commit", git.commit);
    },
        [&p](const RemoteFile &rf)
    {
        if (rf.url.empty())
            return;
        p.add("source.remote.url", rf.url);
    },
        [&p](const RemoteFiles &rfs)
    {
        if (rfs.urls.empty())
            return;
        ptree children;
        for (auto &rf : rfs.urls)
        {
            ptree c;
            c.put("url", rf);
            children.push_back(std::make_pair("", c));
        }
        p.add_child("source.files", children);
    }
    );
    return boost::apply_visitor(write_json, source);
}

String print_source(const Source &source)
{
    auto write_string = overload(
        [](const Git &git)
    {
        String r = "git:\n";
        if (git.empty())
            return r;
        r += "url: " + git.url + "\n";
        if (!git.tag.empty())
            r += "tag: " + git.tag + "\n";
        if (!git.branch.empty())
            r += "branch: " + git.branch + "\n";
        if (!git.commit.empty())
            r += "commit: " + git.commit + "\n";
        return r;
    },
        [](const RemoteFile &rf)
    {
        String r = "remote:\n";
        if (rf.url.empty())
            return r;
        r += "url: " + rf.url + "\n";
        return r;
    },
        [](const RemoteFiles &rfs)
    {
        String r = "files:\n";
        if (rfs.urls.empty())
            return r;
        for (auto &rf : rfs.urls)
            r += "url: " + rf + "\n";
        return r;
    }
    );
    return boost::apply_visitor(write_string, source);
}
