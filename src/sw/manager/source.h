// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "version.h"
#include "property_tree.h"

#include <nlohmann/json_fwd.hpp>
#include <variant>

namespace YAML { class Node; }
using yaml = YAML::Node;

namespace sw
{

using SourceKvMap = std::vector<std::pair<String, String>>;

inline namespace source
{

// prepare for oop
enum class SourceType
{
    Undefined, // remove?

    Empty, // sometimes we have everything in the config file
    Git,
    Mercurial, // Hg also?
    Bazaar,
    Fossil,
    Cvs,
    Svn,
    RemoteFile,
    RemoteFiles,

    //darcs,
    //perforce aka p4

    // do not add local files
};

struct SW_MANAGER_API Source
{
    virtual ~Source() = default;

    /// download subject to destination directory
    virtual void download(const path &dir) const = 0;

    /// save to global object with 'source' subobject
    virtual void save(ptree &p) const = 0;

    /// save to current (passed) object
    virtual void save(nlohmann::json &j) const = 0;

    /// save to global object with 'source' subobject
    virtual void save(yaml &root) const = 0;

    virtual String print() const = 0;
    virtual SourceKvMap printKv() const = 0;
    virtual String getHash() const = 0;
    virtual bool isValidSourceUrl() const = 0;

    ///
    virtual void applyVersion(const Version &v) = 0;

    /// load from global object with 'source' subobject
    static std::unique_ptr<Source> load(const ptree &p);

    /// load from current (passed) object
    static std::unique_ptr<Source> load(const nlohmann::json &j);

    /// load from global object with 'source' subobject
    static std::unique_ptr<Source> load(const yaml &root);

protected:
    virtual SourceType getType() const = 0;
    String getString() const;
};

struct SW_MANAGER_API UndefinedSource : Source
{
    void save(ptree &p) const override {}
    void save(nlohmann::json &p) const override {}
    void save(yaml &root) const override {}

    String print() const override { return ""; }
    SourceKvMap printKv() const override;
    void applyVersion(const Version &v) override {}
    void download(const path &dir) const override {}

private:
    SourceType getType() const override { return SourceType::Undefined; }
};

struct SW_MANAGER_API EmptySource : Source
{
    void save(ptree &p) const override {}
    void save(nlohmann::json &p) const override {}
    void save(yaml &root) const override {}

    String print() const override { return ""; }
    SourceKvMap printKv() const override;
    void applyVersion(const Version &v) override {}
    void download(const path &dir) const override {}

private:
    SourceType getType() const override { return SourceType::Empty; }
};

struct SW_MANAGER_API SourceUrl : Source
{
    String url;

    SourceUrl(const String &url);

    void save(ptree &p) const override;
    void save(nlohmann::json &p) const override;
    void save(yaml &root) const override;

    String print() const override;
    void applyVersion(const Version &v) override;

private:
    virtual void checkUrl() const;
};

struct SW_MANAGER_API Git : SourceUrl
{
    String tag;
    String branch;
    String commit;

    Git(const String &url, const String &tag = "", const String &branch = "", const String &commit = "");

    void save(ptree &p) const override;
    void save(nlohmann::json &p) const override;
    void save(yaml &root) const override;

    String print() const override;
    SourceKvMap printKv() const override;
    void applyVersion(const Version &v) override;
    void download(const path &dir) const override;

private:
    SourceType getType() const override { return SourceType::Git; }
};

struct SW_MANAGER_API Hg : Git
{
    int64_t revision;

    Hg(const String &url, const String &tag = "", const String &branch = "", const String &commit = "", int64_t revision = -1);

    void save(ptree &p) const override;
    void save(nlohmann::json &p) const override;
    void save(yaml &root) const override;

    String print() const override;
    SourceKvMap printKv() const override;
    void applyVersion(const Version &v) override;
    void download(const path &dir) const override;

private:
    SourceType getType() const override { return SourceType::Mercurial; }
};

struct SW_MANAGER_API Bzr : SourceUrl
{
    String tag;
    int64_t revision = -1;

    Bzr(const String &url, const String &tag = "", int64_t revision = -1);

    void save(ptree &p) const override;
    void save(nlohmann::json &p) const override;
    void save(yaml &root) const override;

    String print() const override;
    SourceKvMap printKv() const override;
    void applyVersion(const Version &v) override;
    void download(const path &dir) const override;

private:
    SourceType getType() const override { return SourceType::Bazaar; }
};

struct SW_MANAGER_API Fossil : Git
{
    using Git::Git;

    void download(const path &dir) const override;

private:
    SourceType getType() const override { return SourceType::Fossil; }
};

struct SW_MANAGER_API Cvs : SourceUrl
{
    String tag;
    String branch;
    String revision;
    String module;

    Cvs(const String &url, const String &module, const String &tag = "", const String &branch = "", const String &revision = "");

    void save(ptree &p) const override;
    void save(nlohmann::json &p) const override;
    void save(yaml &root) const override;

    String print() const override;
    SourceKvMap printKv() const override;
    void applyVersion(const Version &v) override;
    void download(const path &dir) const override;

private:
    SourceType getType() const override { return SourceType::Cvs; }
    void checkUrl() const override;
};

struct Svn : SourceUrl
{
    String tag;
    String branch;
    int64_t revision = -1;

    Svn(const String &url, const String &tag = "", const String &branch = "", int64_t revision = -1);

    void save(ptree &p) const override;
    void save(nlohmann::json &p) const override;
    void save(yaml &root) const override;

    String print() const override;
    SourceKvMap printKv() const override;
    void applyVersion(const Version &v) override;
    void download(const path &dir) const override;

private:
    SourceType getType() const override { return SourceType::Svn; }
};

struct SW_MANAGER_API RemoteFile : SourceUrl
{
    using SourceUrl::SourceUrl;

    void download(const path &dir) const override;

private:
    SourceType getType() const override { return SourceType::RemoteFile; }
};

struct SW_MANAGER_API RemoteFiles : Source
{
    StringSet urls;

    RemoteFiles(const StringSet &urls);

    void save(ptree &p) const override;
    void save(nlohmann::json &p) const override;
    void save(yaml &root) const override;

    String print() const override;
    SourceKvMap printKv() const override;
    void applyVersion(const Version &v) override;
    void download(const path &dir) const override;

private:
    SourceType getType() const override { return SourceType::RemoteFiles; }
};

} // inline namespace source

//using SourceDirMap = std::unordered_map<Source, path>;
//using SourceDirSet = std::unordered_set<Source>;

struct SourceDownloadOptions
{
    path source_dir;
    path root_dir; // root to download
    bool ignore_existing_dirs = false;
    std::chrono::seconds existing_dirs_age{ 0 };
    bool adjust_root_dir = true;
};

//SW_MANAGER_API
//void download(SourceDirMap &sources, const SourceDownloadOptions &opts = {});

//SW_MANAGER_API
//SourceDirMap download(SourceDirSet &sources, const SourceDownloadOptions &opts = {});

}

namespace std
{

template<> struct hash<::sw::Source>
{
    size_t operator()(const ::sw::Source &p) const
    {
        return hash<String>()(p.print());
    }
};

}
