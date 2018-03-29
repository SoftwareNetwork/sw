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

#include <fmt/format.h>
#include <primitives/command.h>
#include <primitives/overloads.h>
#include <primitives/pack.h>

#include <regex>

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

using primitives::Command;

void applyVersion(String &s, const Version &v)
{
    s = fmt::format(s,
        fmt::arg("M", (int)v.major),
        fmt::arg("m", (int)v.minor),
        fmt::arg("p", (int)v.patch),
        // "t" - tweak?
        fmt::arg("b", v.branch),
        fmt::arg("v", v.toString())
    );
}

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

static int isEmpty(int64_t i)
{
    return i == -1;
}

static int isEmpty(const String &s)
{
    return s.empty();
}

static int checkEmpty()
{
    return 0;
}

template <typename First, typename ... Args>
static int checkEmpty(First &&f, Args && ... args)
{
    return isEmpty(f) + checkEmpty(std::forward<Args>(args)...);
}

template <typename ... Args>
static String checkOne(const String &name, Args && ... args)
{
    int n = checkEmpty(std::forward<Args>(args)...);
    if (n == 0)
        return "No " + name + " sources available";
    if (n > 1)
        return "Only one " + name + " source must be specified";
    return "";
}

SourceUrl::SourceUrl(const yaml &root, const String &name)
{
    YAML_EXTRACT_VAR(root, url, name, String);
}

template <typename ... Args>
bool SourceUrl::checkValid(const String &name, String *error, Args && ... args) const
{
    if (!isValid(name, error))
        return false;
    auto e = checkOne(name, std::forward<Args>(args)...);
    auto ret = e.empty();
    if (!ret && error)
        *error = e;
    return ret;
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

bool Cvs::isValidUrl() const
{
    static const std::regex checkCvs("-d:([a-z0-9_-]+):([a-z0-9_-]+)@(\\S*):(\\S*)");
    if (std::regex_match(url, checkCvs))
        return true;
    return false;
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

void SourceUrl::applyVersion(const Version &v)
{
    ::applyVersion(url, v);
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
        ScopedCurrentPath scp(current_thread_path() / branchPath);

        Command::execute({ "git", "init" });
        Command::execute({ "git", "remote", "add", "origin", url });
        if (!tag.empty())
        {
            Command::execute({ "git", "fetch", "--depth", "1", "origin", "refs/tags/" + tag });
            Command::execute({ "git", "reset", "--hard", "FETCH_HEAD" });
        }
        else if (!branch.empty())
        {
            Command::execute({ "git", "fetch", "--depth", "1", "origin", branch });
            Command::execute({ "git", "reset", "--hard", "FETCH_HEAD" });
        }
        else if (!commit.empty())
        {
            Command::execute({ "git", "fetch" });
            Command::execute({ "git", "checkout", commit });
        }
    });
}

bool Git::isValid(String *error) const
{
    return checkValid(getString(), error, tag, branch, commit);
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

String Git::printCpp() const
{
    String s;
    s += "Git(\"" + url;
    s += "\", \"" + tag;
    if (tag.empty())
    {
        s += "\", \"" + branch;
        if (branch.empty())
            s += "\", \"" + commit;
    }
    s += "\")";
    return s;
}

void Git::applyVersion(const Version &v)
{
    SourceUrl::applyVersion(v);
    ::applyVersion(tag, v);
    ::applyVersion(branch, v);
}

void Git::loadVersion(Version &version)
{
    auto ver = !version.isValid() ? version.toString() : ""s;

    if (ver.empty())
    {
        if (branch.empty() && tag.empty())
        {
            ver = "master";
            version = Version(ver);
        }
        else if (!branch.empty())
        {
            ver = branch;
            try
            {
                // branch may contain bad symbols, so put in try...catch
                version = Version(ver);
            }
            catch (std::exception &)
            {
            }
        }
        else if (!tag.empty())
        {
            ver = tag;
            try
            {
                // tag may contain bad symbols, so put in try...catch
                version = Version(ver);
            }
            catch (std::exception &)
            {
            }
        }
    }

    if (version.isValid() && branch.empty() && tag.empty() && commit.empty())
    {
        if (version.isBranch())
            branch = version.toString();
        else
            tag = version.toString();
    }
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
        Command::execute({ "hg", "clone", url });

        String branchPath = url.substr(url.find_last_of("/") + 1);
        ScopedCurrentPath scp(current_thread_path() / branchPath);

        if (!tag.empty())
            Command::execute({ "hg", "update", tag });
        else if (!branch.empty())
            Command::execute({ "hg", "update", branch });
        else if (!commit.empty())
            Command::execute({ "hg", "update", commit });
        else if (revision != -1)
            Command::execute({ "hg", "update", std::to_string(revision) });
    });
}

bool Hg::isValid(String *error) const
{
    return checkValid(getString(), error, tag, branch, commit, revision);
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

String Hg::printCpp() const
{
    return String();
}

void Hg::loadVersion(Version &version)
{
    auto ver = !version.isValid() ? version.toString() : ""s;

    if (ver.empty())
    {
        if (branch.empty() && tag.empty() && revision == -1)
        {
            ver = "default";
            version = Version(ver);
        }
        else if (!branch.empty())
        {
            ver = branch;
            try
            {
                // branch may contain bad symbols, so put in try...catch
                version = Version(ver);
            }
            catch (std::exception &)
            {
            }
        }
        else if (!tag.empty())
        {
            ver = tag;
            try
            {
                // tag may contain bad symbols, so put in try...catch
                version = Version(ver);
            }
            catch (std::exception &)
            {
            }
        }
        else if (revision != -1)
        {
            ver = "revision: " + std::to_string(revision);
            try
            {
                // tag may contain bad symbols, so put in try...catch
                version = Version(ver);
            }
            catch (std::exception &)
            {
            }
        }
    }

    if (version.isValid() && branch.empty() && tag.empty() && commit.empty() && revision == -1)
    {
        if (version.isBranch())
            branch = version.toString();
        else
            tag = version.toString();
    }
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
        Command::execute({ "bzr", "branch", url });

        String branchPath = url.substr(url.find_last_of("/") + 1);
        ScopedCurrentPath scp(current_thread_path() / branchPath);

        if (!tag.empty())
            Command::execute({ "bzr", "update", "-r", "tag:" + tag });
        else if (revision != -1)
            Command::execute({ "bzr", "update", "-r", std::to_string(revision) });
    });
}

bool Bzr::isValid(String *error) const
{
    return checkValid(getString(), error, tag, revision);
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

String Bzr::printCpp() const
{
    return String();
}

void Bzr::loadVersion(Version &version)
{
    auto ver = !version.isValid() ? version.toString() : ""s;

    if (ver.empty())
    {
        if (tag.empty() && revision == -1)
        {
            ver = "trunk";
            version = Version(ver);
        }
        else if (!tag.empty())
        {
            ver = tag;
            try
            {
                // tag may contain bad symbols, so put in try...catch
                version = Version(ver);
            }
            catch (std::exception &)
            {
            }
        }
        else if (revision != -1)
        {
            ver = "revision: " + std::to_string(revision);
            try
            {
                // tag may contain bad symbols, so put in try...catch
                version = Version(ver);
            }
            catch (std::exception &)
            {
            }
        }
    }

    if (version.isValid() && tag.empty() && revision == -1)
    {
        tag = version.toString();
    }
}

Fossil::Fossil(const yaml &root, const String &name)
    : Git(root, name)
{
}

void Fossil::download() const
{
    downloadRepository([this]()
    {
        Command::execute({ "fossil", "clone", url, "temp.fossil" });

        fs::create_directory("temp");
        ScopedCurrentPath scp(current_thread_path() / "temp");

        Command::execute({ "fossil", "open", "../temp.fossil" });

        if (!tag.empty())
            Command::execute({ "fossil", "update", tag });
        else if (!branch.empty())
            Command::execute({ "fossil", "update", branch });
        else if (!commit.empty())
            Command::execute({ "fossil", "update", commit });
    });
}

void Fossil::save(yaml &root, const String &name) const
{
    Git::save(root, name);
}

void Fossil::loadVersion(Version &version)
{
    auto ver = !version.isValid() ? version.toString() : ""s;

    if (ver.empty())
    {
        if (branch.empty() && tag.empty())
        {
            ver = "trunk";
            version = Version(ver);
        }
        else if (!branch.empty())
        {
            ver = branch;
            try
            {
                // branch may contain bad symbols, so put in try...catch
                version = Version(ver);
            }
            catch (std::exception &)
            {
            }
        }
        else if (!tag.empty())
        {
            ver = tag;
            try
            {
                // tag may contain bad symbols, so put in try...catch
                version = Version(ver);
            }
            catch (std::exception &)
            {
            }
        }
    }

    if (version.isValid() && branch.empty() && tag.empty() && commit.empty())
    {
        if (version.isBranch())
            branch = version.toString();
        else
            tag = version.toString();
    }
}

Cvs::Cvs(const yaml &root, const String &name)
    : SourceUrl(root, name)
{
    YAML_EXTRACT_AUTO(tag);
    YAML_EXTRACT_AUTO(branch);
    YAML_EXTRACT_AUTO(revision);
    YAML_EXTRACT_AUTO(module);
}

void Cvs::download() const
{
    downloadRepository([this]()
    {
		Command::execute({ "cvs", url, "co", module });

        ScopedCurrentPath scp(current_thread_path() / module, CurrentPathScope::All);

		if (!tag.empty())
			Command::execute({ "cvs", "update", "-r", tag });
		else if (!branch.empty())
            Command::execute({ "cvs", "update", "-r", branch });
		else if (!revision.empty())
            Command::execute({ "cvs", "update", "-r", revision });
    });
}

bool Cvs::isValid(String *error) const
{
    return checkValid(getString(), error, tag, branch, revision);
}

bool Cvs::load(const ptree &p)
{
    if (!SourceUrl::load(p))
        return false;
    PTREE_GET_STRING(tag);
    PTREE_GET_STRING(branch);
    PTREE_GET_STRING(revision);
    PTREE_GET_STRING(module);
    return true;
}

bool Cvs::save(ptree &p) const
{
    if (!SourceUrl::save(p))
        return false;
    PTREE_ADD_NOT_EMPTY(tag);
    PTREE_ADD_NOT_EMPTY(branch);
    PTREE_ADD_NOT_EMPTY(revision);
    return true;
}

void Cvs::save(yaml &root, const String &name) const
{
    SourceUrl::save(root, name);
    YAML_SET_NOT_EMPTY(tag);
    YAML_SET_NOT_EMPTY(branch);
    YAML_SET_NOT_EMPTY(revision);
}

String Cvs::print() const
{
    auto r = SourceUrl::print();
    if (r.empty())
        return r;
    STRING_PRINT_NOT_EMPTY(tag);
    STRING_PRINT_NOT_EMPTY(branch);
    STRING_PRINT_NOT_EMPTY(revision);
    STRING_PRINT_NOT_EMPTY(module);
    return r;
}

String Cvs::printCpp() const
{
    return String();
}

void Cvs::loadVersion(Version &version)
{
    auto ver = !version.isValid() ? version.toString() : ""s;

    if (ver.empty())
    {
        if (branch.empty() && tag.empty() && revision.empty())
        {
            ver = "trunk";
            version = Version(ver);
        }
        else if (!branch.empty())
        {
            ver = branch;
            try
            {
                // branch may contain bad symbols, so put in try...catch
                version = Version(ver);
            }
            catch (std::exception &)
            {
            }
        }
        else if (!tag.empty())
        {
            ver = tag;
            try
            {
                // tag may contain bad symbols, so put in try...catch
                version = Version(ver);
            }
            catch (std::exception &)
            {
            }
        }
        else if (!revision.empty())
        {
            ver = revision;
            try
            {
                // tag may contain bad symbols, so put in try...catch
                version = Version(ver);
            }
            catch (std::exception &)
            {
            }
        }
    }

    if (version.isValid() && branch.empty() && tag.empty() && revision.empty())
    {
        if (version.isBranch())
            branch = version.toString();
        else
            tag = version.toString();
    }
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

String RemoteFile::printCpp() const
{
    String s;
    s += "RemoteFile(\"" + url;
    s += "\")";
    return s;
}

void RemoteFile::applyVersion(const Version &v)
{
    ::applyVersion(url, v);
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

String RemoteFiles::printCpp() const
{
    String s;
    s += "RemoteFiles(";
    for (auto &rf : urls)
        s += "\"" + rf + "\", ";
    s.resize(s.size() - 2);
    s += ")";
    return s;
}

void RemoteFiles::applyVersion(const Version &v)
{
    decltype(urls) urls2;
    for (auto &rf : urls)
    {
        auto u = rf;
        ::applyVersion(u, v);
        urls2.insert(u);
    }
    urls = urls2;
}

void download(const Source &source, int64_t max_file_size)
{
    visit([](auto &v) { v.download(); }, source);
}

bool isValidSourceUrl(const Source &source)
{
    return visit([](auto &v) { return v.isValidUrl(); }, source);
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
    for (auto &i : sources)
    {
        YAML_EXTRACT_VAR(src, s, i, String);
        if (!s.empty())
        {
            s = i;
            break;
        }
    }

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
    visit([&root](auto &v) { auto r = root["source"]; v.save(r); }, source);
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
    return visit([&p](auto &v)
    {
        ptree p2;
        v.save(p2);
        p.add_child("source." + v.getString(), p2);
    }, source);
}

String print_source(const Source &source)
{
    return visit([](auto &v) { return v.getString() + ":\n" + v.print(); }, source);
}

String print_source_cpp(const Source &source)
{
    return visit([](auto &v) { return v.printCpp(); }, source);
}

void applyVersionToUrl(Source &source, const Version &v)
{
    visit([&v](auto &s) { s.applyVersion(v); }, source);
}
