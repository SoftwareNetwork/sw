// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "source.h"

#include "hash.h"
#include "http.h"
#include "yaml.h"

#include <primitives/command.h>
#include <primitives/filesystem.h>
#include <primitives/overload.h>
#include <primitives/pack.h>

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <regex>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "source");

#define JSON_ADD(x) j[#x] = x
#define JSON_ADD_NOT_EMPTY(x) if (!x.empty()) JSON_ADD(x)
#define JSON_ADD_NOT_MINUS_ONE(x) if (x != -1) JSON_ADD(x)

#define JSON_GET_STRING(x) if (j.find(#x) != j.end()) x = j[#x].get<std::string>()
#define JSON_GET_INT(x) if (j.find(#x) != j.end()) x = j[#x].get<int64_t>()

#define PTREE_ADD(x) p.add(#x, x)
#define PTREE_ADD_NOT_EMPTY(x) if (!x.empty()) PTREE_ADD(x)
#define PTREE_ADD_NOT_MINUS_ONE(x) if (x != -1) PTREE_ADD(x)

#define PTREE_GET_STRING(x) x = p.get(#x, "")
#define PTREE_GET_INT(x) x = p.get(#x, -1LL)

#define STRING_PRINT_VALUE(x, v) r += #x ": " + v + "\n"
#define STRING_PRINT(x) STRING_PRINT_VALUE(#x, x)
#define STRING_PRINT_NOT_EMPTY(x) if (!x.empty()) STRING_PRINT(x)
#define STRING_PRINT_NOT_MINUS_ONE(x) if (x != -1) STRING_PRINT_VALUE(#x, std::to_string(x))

#define YAML_SET(x, n) root[n] = x
#define YAML_SET_NOT_EMPTY(x) if (!x.empty()) YAML_SET(x, #x)
#define YAML_SET_NOT_MINUS_ONE(x) if (x != -1) YAML_SET(x, #x)

using primitives::Command;

namespace sw
{

static void download_file_checked(const String &url, const path &fn, int64_t max_file_size = 0)
{
    checkSourceUrl(url);
    download_file(url, fn, max_file_size);
}

static void download_and_unpack(const String &url, path fn, int64_t max_file_size = 0)
{
    if (!fn.is_absolute())
        fn = current_thread_path() / fn;
    download_file_checked(url, fn, max_file_size);
    unpack_file(fn, current_thread_path());
    fs::remove(fn);
}

template <typename F>
static void downloadRepository(F &&f)
{
    std::exception_ptr eptr;
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
            if (!eptr)
                eptr = std::current_exception();
            if (n_tries == 0)
                std::rethrow_exception(eptr);
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

SourceUrl::SourceUrl(const String &url)
    : url(url)
{
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
    return ::isValidSourceUrl(url);
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

bool SourceUrl::load(const nlohmann::json &j)
{
    JSON_GET_STRING(url);
    return !empty();
}

bool SourceUrl::save(nlohmann::json &j) const
{
    if (empty())
        return false;
    JSON_ADD(url);
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
    v.format(url);
}

Git::Git(const yaml &root, const String &name)
    : SourceUrl(root, name)
{
    YAML_EXTRACT_AUTO(tag);
    YAML_EXTRACT_AUTO(branch);
    YAML_EXTRACT_AUTO(commit);
}

Git::Git(const String &url, const String &tag, const String &branch, const String &commit)
    : SourceUrl(url), tag(tag), branch(branch), commit(commit)
{
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
            github_url = github_url.substr(0, github_url.size() - suffix.size());

        path fn;
        github_url += "/archive/";
        if (!tag.empty())
        {
            github_url += make_archive_name(tag);
            fn = current_thread_path() / make_archive_name("1");
        }
        else if (!branch.empty())
        {
            github_url += branch + ".zip"; // but use .zip for branches!
            fn = current_thread_path() / "1.zip";
        }
        else if (!commit.empty())
        {
            github_url += commit + ".zip"; // but use .zip for branches!
            fn = current_thread_path() / "1.zip";
        }

        try
        {
            download_and_unpack(github_url, fn);
            return;
        }
        catch (std::exception &e)
        {
            // go to usual git download
            LOG_WARN(logger, e.what());
            fs::remove(fn);
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
        auto p = current_thread_path() / branchPath;
        fs::create_directories(p);

        Command::execute({ "git", "-C", p.string(), "init" });
        Command::execute({ "git", "-C", p.string(), "remote", "add", "origin", url });
        if (!tag.empty())
        {
            Command::execute({ "git", "-C", p.string(), "fetch", "--depth", "1", "origin", "refs/tags/" + tag });
            Command::execute({ "git", "-C", p.string(), "reset", "--hard", "FETCH_HEAD" });
        }
        else if (!branch.empty())
        {
            Command::execute({ "git", "-C", p.string(), "fetch", "--depth", "1", "origin", branch });
            Command::execute({ "git", "-C", p.string(), "reset", "--hard", "FETCH_HEAD" });
        }
        else if (!commit.empty())
        {
            Command::execute({ "git", "-C", p.string(), "fetch" });
            Command::execute({ "git", "-C", p.string(), "checkout", commit });
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

bool Git::load(const nlohmann::json &j)
{
    if (!SourceUrl::load(j))
        return false;
    JSON_GET_STRING(tag);
    JSON_GET_STRING(branch);
    JSON_GET_STRING(commit);
    return !empty();
}

bool Git::save(nlohmann::json &j) const
{
    if (!SourceUrl::save(j))
        return false;
    JSON_ADD_NOT_EMPTY(tag);
    JSON_ADD_NOT_EMPTY(branch);
    JSON_ADD_NOT_EMPTY(commit);
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

void Git::applyVersion(const Version &v)
{
    SourceUrl::applyVersion(v);
    v.format(tag);
    v.format(branch);
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
        ScopedCurrentPath scp(fs::current_path() / branchPath);

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

bool Hg::load(const nlohmann::json &j)
{
    if (!Git::load(j))
        return false;
    JSON_GET_INT(revision);
    return !empty();
}

bool Hg::save(nlohmann::json &j) const
{
    if (!Git::save(j))
        return false;
    JSON_ADD_NOT_MINUS_ONE(revision);
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
        Command::execute({ "bzr", "branch", url });

        String branchPath = url.substr(url.find_last_of("/") + 1);
        ScopedCurrentPath scp(fs::current_path() / branchPath);

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

bool Bzr::load(const nlohmann::json &j)
{
    if (!SourceUrl::load(j))
        return false;
    JSON_GET_STRING(tag);
    JSON_GET_INT(revision);
    return !empty();
}

bool Bzr::save(nlohmann::json &j) const
{
    if (!SourceUrl::save(j))
        return false;
    JSON_ADD_NOT_EMPTY(tag);
    JSON_ADD_NOT_MINUS_ONE(revision);
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
        Command::execute({ "fossil", "clone", url, "temp.fossil" });

        fs::create_directories("temp");
        ScopedCurrentPath scp(fs::current_path() / "temp");

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

Cvs::Cvs(const yaml &root, const String &name)
    : SourceUrl(root, name)
{
    YAML_EXTRACT_AUTO(tag);
    YAML_EXTRACT_AUTO(branch);
    YAML_EXTRACT_AUTO(revision);
    YAML_EXTRACT_AUTO(module);
}

bool Cvs::isValidUrl() const
{
    static const std::regex checkCvs("-d:([a-z0-9_-]+):([a-z0-9_-]+)@(\\S*):(\\S*)");
    if (std::regex_match(url, checkCvs))
        return true;
    return false;
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
    PTREE_ADD_NOT_EMPTY(module);
    return true;
}

bool Cvs::load(const nlohmann::json &j)
{
    if (!SourceUrl::load(j))
        return false;
    JSON_GET_STRING(tag);
    JSON_GET_STRING(branch);
    JSON_GET_STRING(revision);
    JSON_GET_STRING(module);
    return !empty();
}

bool Cvs::save(nlohmann::json &j) const
{
    if (!SourceUrl::save(j))
        return false;
    JSON_ADD_NOT_EMPTY(tag);
    JSON_ADD_NOT_EMPTY(branch);
    JSON_ADD_NOT_EMPTY(revision);
    JSON_ADD_NOT_EMPTY(module);
    return true;
}

void Cvs::save(yaml &root, const String &name) const
{
    SourceUrl::save(root, name);
    YAML_SET_NOT_EMPTY(tag);
    YAML_SET_NOT_EMPTY(branch);
    YAML_SET_NOT_EMPTY(revision);
    YAML_SET_NOT_EMPTY(module);
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

Svn::Svn(const yaml &root, const String &name)
    : SourceUrl(root, name)
{
    YAML_EXTRACT_AUTO(tag);
    YAML_EXTRACT_AUTO(branch);
    YAML_EXTRACT_AUTO(revision);
}

void Svn::download() const
{
    downloadRepository([this]()
    {
        if (!tag.empty())
            Command::execute({ "svn", "checkout", url + "/tags/" + tag }); //tag
        else if (!branch.empty())
            Command::execute({ "svn", "checkout", url + "/branches/" + branch }); //branch
        else if (revision != -1)
            Command::execute({ "svn", "checkout", "-r", std::to_string(revision), url });
        else
            Command::execute({ "svn", "checkout", url + "/trunk" });
    });
}

bool Svn::isValid(String *error) const
{
    return checkValid(getString(), error, tag, branch, revision);
}

bool Svn::load(const ptree &p)
{
    if (!SourceUrl::load(p))
        return false;
    PTREE_GET_STRING(tag);
    PTREE_GET_STRING(branch);
    PTREE_GET_INT(revision);
    return true;
}

bool Svn::save(ptree &p) const
{
    if (!SourceUrl::save(p))
        return false;
    PTREE_ADD_NOT_EMPTY(tag);
    PTREE_ADD_NOT_EMPTY(branch);
    PTREE_ADD_NOT_MINUS_ONE(revision);
    return true;
}

bool Svn::load(const nlohmann::json &j)
{
    if (!SourceUrl::load(j))
        return false;
    JSON_GET_STRING(tag);
    JSON_GET_STRING(branch);
    JSON_GET_INT(revision);
    return !empty();
}

bool Svn::save(nlohmann::json &j) const
{
    if (!SourceUrl::save(j))
        return false;
    JSON_ADD_NOT_EMPTY(tag);
    JSON_ADD_NOT_EMPTY(branch);
    JSON_ADD_NOT_MINUS_ONE(revision);
    return true;
}

void Svn::save(yaml &root, const String &name) const
{
    SourceUrl::save(root, name);
    YAML_SET_NOT_EMPTY(tag);
    YAML_SET_NOT_EMPTY(branch);
    YAML_SET_NOT_MINUS_ONE(revision);
}

String Svn::print() const
{
    auto r = SourceUrl::print();
    if (r.empty())
        return r;
    STRING_PRINT_NOT_EMPTY(tag);
    STRING_PRINT_NOT_EMPTY(branch);
    STRING_PRINT_NOT_MINUS_ONE(revision);
    return r;
}

String Svn::printCpp() const
{
    return String();
}

RemoteFile::RemoteFile(const String &url)
    : SourceUrl(url)
{
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

void RemoteFile::applyVersion(const Version &v)
{
    v.format(url);
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
        [](auto &u) { return ::isValidSourceUrl(u); });
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

bool RemoteFiles::load(const nlohmann::json &j)
{
    Strings s = j["url"];
    urls.insert(s.begin(), s.end());
    return !empty();
}

bool RemoteFiles::save(nlohmann::json &j) const
{
    if (empty())
        return false;
    for (auto &rf : urls)
        j["url"].push_back(rf);
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

void RemoteFiles::applyVersion(const Version &v)
{
    decltype(urls) urls2;
    for (auto &rf : urls)
    {
        auto u = rf;
        v.format(u);
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

String get_source_hash(const Source &source)
{
    return sha256_short(print_source(source));
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

Source load_source(const nlohmann::json &j)
{
#define TRY_TO_LOAD_SOURCE(x)              \
    if (j.find(x::getString()) != j.end()) \
    {                                      \
        x x##_;                            \
        x##_.load(j[x::getString()]);      \
        return x##_;                       \
    }
    SOURCE_TYPES(TRY_TO_LOAD_SOURCE, DELIM_SEMICOLON);
    throw std::runtime_error("Bad source");
}

void save_source(nlohmann::json &j, const Source &source)
{
    return visit([&j](auto &v) { v.save(j[v.getString()]); }, source);
}

String print_source(const Source &source)
{
    return visit([](auto &v) { return v.getString() + ":\n" + v.print(); }, source);
}

void applyVersionToUrl(Source &source, const Version &v)
{
    visit([&v](auto &s) { s.applyVersion(v); }, source);
}

}
