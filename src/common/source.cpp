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
#include "yaml.h"

#include <primitives/overloads.h>
#include <primitives/pack.h>

#define PTREE_ADD(x) p.add(#x, x)
#define PTREE_ADD_NOT_EMPTY(x) if (!x.empty()) PTREE_ADD(x)
#define PTREE_ADD_NOT_MINUS_ONE(x) if (x != -1) PTREE_ADD(x)

#define PTREE_GET_STRING(x) x = p.get(#x, "")
#define PTREE_GET_INT(x) x = p.get(#x, -1)

#define STRING_PRINT_VALUE(x, v) r += #x ": " + v + "\n"
#define STRING_PRINT(x) STRING_PRINT_VALUE(#x, x)
#define STRING_PRINT_NOT_EMPTY(x) if (!x.empty()) STRING_PRINT(x)
#define STRING_PRINT_NOT_MINUS_ONE(x) if (x != -1) STRING_PRINT_VALUE(#x, std::to_string(x))

#define YAML_SET(x, n) root[n] = x
#define YAML_SET_NOT_EMPTY(x) if (!x.empty()) YAML_SET(x, #x)
#define YAML_SET_NOT_MINUS_ONE(x) if (x != -1) YAML_SET(x, #x)

static void download_file_checked(const String &url, const path &fn, int64_t max_file_size = 0)
{
    checkSourceUrl(url);
    download_file(url, fn, max_file_size);
}

static void download_and_unpack(const String &url, const path &fn, int64_t max_file_size = 0)
{
    download_file_checked(url, fn, max_file_size);
    unpack_file(fn, ".");
    fs::remove(fn);
}

template <typename F>
static void downloadRepository(F &&f)
{
    int n_tries = 3;
    while (n_tries--)
    {
        try
        {
            f();
            break;
        }
        catch (...)
        {
            if (n_tries == 0)
                throw;
        }
    }
}

static void run(const String &c)
{
    if (std::system(c.c_str()) != 0)
        throw std::runtime_error("Last command failed: " + c);
}

SourceUrl::SourceUrl(const yaml &root, const String &name)
{
    YAML_EXTRACT_VAR(root, url, name, String);
}

bool SourceUrl::isValid(const String &name, String *error) const
{
    if (!empty())
        return true;
    if (error)
        *error = name + " url is missing";
    return false;
}

bool SourceUrl::isValidUrl() const
{
    return isValidSourceUrl(url);
}

bool SourceUrl::load(const ptree &p)
{
    PTREE_GET_STRING(url);
    return !empty();
}

bool SourceUrl::save(ptree &p) const
{
    if (empty())
        return false;
    PTREE_ADD(url);
    return true;
}

void SourceUrl::save(yaml &root, const String &name) const
{
    YAML_SET(url, name);
}

String SourceUrl::print() const
{
    String r;
    if (empty())
        return r;
    STRING_PRINT(url);
    return r;
}

Git::Git(const yaml &root, const String &name)
    : SourceUrl(root, name)
{
    YAML_EXTRACT_AUTO(tag);
    YAML_EXTRACT_AUTO(branch);
    YAML_EXTRACT_AUTO(commit);
}

void Git::download() const
{
    // try to speed up git downloads from github
    // add more sites below
    if (url.find("github.com") != url.npos)
    {
        auto github_url = url;

        // remove possible .git suffix
        String suffix = ".git";
        if (github_url.rfind(suffix) == github_url.size() - suffix.size())
            github_url.substr(0, github_url.size() - suffix.size());

        String fn;
        github_url += "/archive/";
        if (!tag.empty())
        {
            github_url += make_archive_name(tag);
            fn = make_archive_name("1");
        }
        else if (!branch.empty())
        {
            github_url += branch + ".zip"; // but use .zip for branches!
            fn = "1.zip";
        }
        else if (!commit.empty())
        {
            github_url += commit + ".zip"; // but use .zip for branches!
            fn = "1.zip";
        }

        try
        {
            download_and_unpack(github_url, fn);
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

    downloadRepository([this]()
    {
        String branchPath = url.substr(url.find_last_of("/") + 1);
        fs::create_directory(branchPath);
        ScopedCurrentPath scp(fs::current_path() / branchPath);

        run("git init");
        run("git remote add origin " + url);
        if (!tag.empty())
        {
            run("git fetch --depth 1 origin refs/tags/" + tag);
            run("git reset --hard FETCH_HEAD");
        }
        else if (!branch.empty())
        {
            run("git fetch --depth 1 origin " + branch);
            run("git reset --hard FETCH_HEAD");
        }
        else if (!commit.empty())
        {
            run("git fetch");
            run("git checkout " + commit);
        }
    });
}

bool Git::isValid(String *error) const
{
    if (!SourceUrl::isValid(getString(), error))
        return false;

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

bool Git::load(const ptree &p)
{
    if (!SourceUrl::load(p))
        return false;
    PTREE_GET_STRING(tag);
    PTREE_GET_STRING(branch);
    PTREE_GET_STRING(commit);
    return true;
}

bool Git::save(ptree &p) const
{
    if (!SourceUrl::save(p))
        return false;
    PTREE_ADD_NOT_EMPTY(tag);
    PTREE_ADD_NOT_EMPTY(branch);
    PTREE_ADD_NOT_EMPTY(commit);
    return true;
}

void Git::save(yaml &root, const String &name) const
{
    SourceUrl::save(root, name);
    YAML_SET_NOT_EMPTY(tag);
    YAML_SET_NOT_EMPTY(branch);
    YAML_SET_NOT_EMPTY(commit);
}

String Git::print() const
{
    auto r = SourceUrl::print();
    if (r.empty())
        return r;
    STRING_PRINT_NOT_EMPTY(tag);
    STRING_PRINT_NOT_EMPTY(branch);
    STRING_PRINT_NOT_EMPTY(commit);
    return r;
}

Hg::Hg(const yaml &root, const String &name)
    : Git(root, name)
{
    YAML_EXTRACT_AUTO(revision);
}

void Hg::download() const
{
    downloadRepository([this]()
    {
        run("hg clone " + url);

        String branchPath = url.substr(url.find_last_of("/") + 1);
        ScopedCurrentPath scp(fs::current_path() / branchPath);

        if (!tag.empty())
            run("hg update " + tag);
        else if (!branch.empty())
            run("hg update " + branch);
        else if (!commit.empty())
            run("hg update " + commit);
        else if (revision != -1)
            run("hg update " + std::to_string(revision));
    });
}

bool Hg::isValid(String *error) const
{
    if (!SourceUrl::isValid(getString(), error))
        return false;

    int e = 0;
    e += !tag.empty();
    e += !branch.empty();
    e += !commit.empty();
    e += revision != -1;

    if (e == 0)
    {
        if (error)
            *error = "No hg sources (tag or branch or commit or revision) available";
        return false;
    }

    if (e > 1)
    {
        if (error)
            *error = "Only one hg source (tag or branch or commit or revision) must be specified";
        return false;
    }

    return true;
}

bool Hg::load(const ptree &p)
{
    if (!Git::load(p))
        return false;
    PTREE_GET_INT(revision);
    return true;
}

bool Hg::save(ptree &p) const
{
    if (!Git::save(p))
        return false;
    PTREE_ADD_NOT_MINUS_ONE(revision);
    return true;
}

void Hg::save(yaml &root, const String &name) const
{
    Git::save(root, name);
    YAML_SET_NOT_MINUS_ONE(revision);
}

String Hg::print() const
{
    auto r = Git::print();
    if (r.empty())
        return r;
    STRING_PRINT_NOT_MINUS_ONE(revision);
    return r;
}

Bzr::Bzr(const yaml &root, const String &name)
    : SourceUrl(root, name)
{
    YAML_EXTRACT_AUTO(tag);
    YAML_EXTRACT_AUTO(revision);
}

void Bzr::download() const
{
    downloadRepository([this]()
    {
        run("bzr branch " + url);

        String branchPath = url.substr(url.find_last_of("/") + 1);
        ScopedCurrentPath scp(fs::current_path() / branchPath);

        if (!tag.empty())
            run("bzr update -r tag:" + tag);
        else if (revision != -1)
            run("bzr update -r " + std::to_string(revision));
    });
}

bool Bzr::isValid(String *error) const
{
    if (!SourceUrl::isValid(getString(), error))
        return false;

    int e = 0;
    e += !tag.empty();
    e += revision != -1;

    if (e == 0)
    {
        if (error)
            *error = "No bzr sources (tag or revision) available";
        return false;
    }

    if (e > 1)
    {
        if (error)
            *error = "Only one Bzr source (tag or revision) must be specified";
        return false;
    }

    return true;
}

bool Bzr::load(const ptree &p)
{
    if (!SourceUrl::load(p))
        return false;
    PTREE_GET_STRING(tag);
    PTREE_GET_INT(revision);
    return true;
}

bool Bzr::save(ptree &p) const
{
    if (!SourceUrl::save(p))
        return false;
    PTREE_ADD_NOT_EMPTY(tag);
    PTREE_ADD_NOT_MINUS_ONE(revision);
    return true;
}

void Bzr::save(yaml &root, const String &name) const
{
    SourceUrl::save(root, name);
    YAML_SET_NOT_EMPTY(tag);
    YAML_SET_NOT_MINUS_ONE(revision);
}

String Bzr::print() const
{
    auto r = SourceUrl::print();
    if (r.empty())
        return r;
    STRING_PRINT_NOT_EMPTY(tag);
    STRING_PRINT_NOT_MINUS_ONE(revision);
    return r;
}

Fossil::Fossil(const yaml &root, const String &name)
    : Git(root, name)
{
}

void Fossil::download() const
{
    downloadRepository([this]()
    {
        run("fossil clone " + url + " " + "temp.fossil");

        fs::create_directory("temp");
        ScopedCurrentPath scp(fs::current_path() / "temp");

        run("fossil open ../temp.fossil");

        if (!tag.empty())
            run("fossil update " + tag);
        else if (!branch.empty())
            run("fossil update " + branch);
        else if (!commit.empty())
            run("fossil update " + commit);
    });
}

bool Fossil::isValid(String *error) const
{
    if (!SourceUrl::isValid(getString(), error))
        return false;

    int e = 0;
    e += !tag.empty();
    e += !branch.empty();
    e += !commit.empty();

    if (e == 0)
    {
        if (error)
            *error = "No fossil sources (tag or branch or commit) available";
        return false;
    }

    if (e > 1)
    {
        if (error)
            *error = "Only one fossil source (tag or branch or commit) must be specified";
        return false;
    }

    return true;
}

void Fossil::save(yaml &root, const String &name) const
{
    Git::save(root, name);
}

RemoteFile::RemoteFile(const yaml &root, const String &name)
    : SourceUrl(root, name)
{
    if (url.empty())
        throw std::runtime_error("Remote url is missing");
}

void RemoteFile::download() const
{
    download_and_unpack(url, path(url).filename());
}

void RemoteFile::save(yaml &root, const String &name) const
{
    SourceUrl::save(root, name);
}

RemoteFiles::RemoteFiles(const yaml &root, const String &name)
{
    urls = get_sequence_set<String>(root, name);
    if (urls.empty())
        throw std::runtime_error("Empty remote files");
}

void RemoteFiles::download() const
{
    for (auto &rf : urls)
        download_file_checked(rf, path(rf).filename());
}

bool RemoteFiles::isValidUrl() const
{
    return std::all_of(urls.begin(), urls.end(),
        [](auto &u) { return isValidSourceUrl(u); });
}

bool RemoteFiles::load(const ptree &p)
{
    for (auto &url : p)
        urls.insert(url.second.get("url", ""s));
    return !empty();
}

bool RemoteFiles::save(ptree &p) const
{
    if (empty())
        return false;
    for (auto &rf : urls)
    {
        ptree c;
        c.put("url", rf);
        p.push_back(std::make_pair("", c));
    }
    return true;
}

void RemoteFiles::save(yaml &root, const String &name) const
{
    for (auto &rf : urls)
        root[name].push_back(rf);
}

String RemoteFiles::print() const
{
    String r;
    if (empty())
        return r;
    for (auto &rf : urls)
        STRING_PRINT_VALUE(url, rf);
    return r;
}

void download(const Source &source, int64_t max_file_size)
{
    boost::apply_visitor([](auto &v) { v.download(); }, source);
}

bool isValidSourceUrl(const Source &source)
{
    return boost::apply_visitor([](auto &v) { return v.isValidUrl(); }, source);
}

bool load_source(const yaml &root, Source &source)
{
    static const auto sources =
    {
#define GET_STRING(x) x::getString()
        SOURCE_TYPES(GET_STRING, DELIM_COMMA)
    };

    auto &src = root["source"];
    if (!src.IsDefined())
        return false;

    String s;
    std::any_of(sources.begin(), sources.end(), [&src, &s](auto &i)
    {
        YAML_EXTRACT_VAR(src, s, i, String);
        if (!s.empty())
        {
            s = i;
            return true;
        }
        return false;
    });

    if (0);
#define IF_SOURCE(x) else if (s == x::getString()) source = x(src)
    SOURCE_TYPES(IF_SOURCE, DELIM_SEMICOLON);
    else
        throw std::runtime_error("Empty source");
    return true;
}

void save_source(yaml &root, const Source &source)
{
    // do not remove 'r' var, it creates 'source' key
    boost::apply_visitor([&root](auto &v) { auto r = root["source"]; v.save(r); }, source);
}

Source load_source(const ptree &p)
{
    auto c = p.get_child("source");
#define TRY_TO_LOAD_SOURCE(x)                               \
    if (c.find(x::getString()) != c.not_found())            \
    {                                                       \
        x x##_;                                             \
        x##_.load(c.get_child(x::getString()));             \
        return x##_;                                        \
    }
    SOURCE_TYPES(TRY_TO_LOAD_SOURCE, DELIM_SEMICOLON);
    throw std::runtime_error("Bad source");
}

void save_source(ptree &p, const Source &source)
{
    return boost::apply_visitor([&p](auto &v)
    {
        ptree p2;
        v.save(p2);
        p.add_child("source." + v.getString(), p2);
    }, source);
}

String print_source(const Source &source)
{
    return boost::apply_visitor([](auto &v) { return v.getString() + ":\n" + v.print(); }, source);
}
