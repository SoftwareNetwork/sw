// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>
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

enum class SourceType
{
#define SOURCE(e, s) e,
#include "source.inl"
#undef SOURCE

    // aliases
    Hg = Mercurial,
    Bzr = Bazaar,
    Empty = EmptySource,
};

struct SW_MANAGER_API Source
{
    virtual ~Source() = default;

    String getHash() const;
    String print() const;
    SourceKvMap printKv() const;

    /// download files to destination directory
    void download(const path &dir) const;

    ///
    std::unique_ptr<Source> clone() const;

    // save

    /// save to current object with 'getString()' subobject
    void save(nlohmann::json &j) const;

    /// save to current object with 'getString()' subobject
    void save(ptree &p) const;

    /// save to current object with 'getString()' subobject
    void save(yaml &root) const;

    // virtual

    ///
    virtual void applyVersion(const Version &v) = 0;

    // static

    /// load from current (passed) object, detects 'getString()' subobject
    static std::unique_ptr<Source> load(const nlohmann::json &j);

    /// load from current (passed) object, detects 'getString()' subobject
    static std::unique_ptr<Source> load(const ptree &p);

    /// load from current (passed) object, detects 'getString()' subobject
    static std::unique_ptr<Source> load(const yaml &root);

protected:
    virtual SourceType getType() const = 0;
    String getString() const;

private:
    virtual String print1() const = 0;
    virtual void download1(const path &dir) const = 0;
    virtual void save1(nlohmann::json &j) const = 0;
    virtual void save1(yaml &root) const = 0;
    virtual void save1(ptree &p) const = 0;
    virtual void printKv(SourceKvMap &) const {}
};

using SourcePtr = std::unique_ptr<Source>;

struct EmptySource : Source
{
    EmptySource() {}

    EmptySource(const nlohmann::json &j) {}
    EmptySource(const ptree &p) {}
    EmptySource(const yaml &root) {}

    void applyVersion(const Version &v) override {}

private:
    SourceType getType() const override { return SourceType::Empty; }
    String print1() const override { return ""; }
    void download1(const path &dir) const override {}
    void save1(nlohmann::json &p) const override {}
    void save1(yaml &root) const override {}
    void save1(ptree &p) const override {}
};

struct SW_MANAGER_API SourceUrl : Source
{
    String url;

    SourceUrl(const String &url);

    SourceUrl(const nlohmann::json &j);
    SourceUrl(const ptree &p);
    SourceUrl(const yaml &root);

    void applyVersion(const Version &v) override;

protected:
    String print1() const override;
    void save1(nlohmann::json &p) const override;
    void save1(yaml &root) const override;
    void save1(ptree &p) const override;
    void printKv(SourceKvMap &) const override;

private:
    virtual void checkUrl() const;
};

struct SW_MANAGER_API Git : SourceUrl
{
    String tag;
    String branch;
    String commit;

    Git(const String &url, const String &tag = "", const String &branch = "", const String &commit = "");

    Git(const nlohmann::json &j);
    Git(const ptree &p);
    Git(const yaml &root);

    void applyVersion(const Version &v) override;

protected:
    String print1() const override;
    void save1(nlohmann::json &p) const override;
    void save1(yaml &root) const override;
    void save1(ptree &p) const override;
    void printKv(SourceKvMap &) const override;

private:
    SourceType getType() const override { return SourceType::Git; }
    void download1(const path &dir) const override;
};

struct SW_MANAGER_API Hg : Git
{
    int64_t revision;

    Hg(const String &url, const String &tag = "", const String &branch = "", const String &commit = "", int64_t revision = -1);

    Hg(const nlohmann::json &j);
    Hg(const ptree &p);
    Hg(const yaml &root);

private:
    SourceType getType() const override { return SourceType::Mercurial; }
    String print1() const override;
    void download1(const path &dir) const override;
    void save1(nlohmann::json &p) const override;
    void save1(yaml &root) const override;
    void save1(ptree &p) const override;
    void printKv(SourceKvMap &) const override;
};

using Mercurial = Hg;

struct SW_MANAGER_API Bzr : SourceUrl
{
    String tag;
    int64_t revision = -1;

    Bzr(const String &url, const String &tag = "", int64_t revision = -1);

    Bzr(const nlohmann::json &j);
    Bzr(const ptree &p);
    Bzr(const yaml &root);

    void applyVersion(const Version &v) override;

private:
    SourceType getType() const override { return SourceType::Bazaar; }
    String print1() const override;
    void download1(const path &dir) const override;
    void save1(nlohmann::json &p) const override;
    void save1(yaml &root) const override;
    void save1(ptree &p) const override;
    void printKv(SourceKvMap &) const override;
};

using Bazaar = Bzr;

struct SW_MANAGER_API Fossil : Git
{
    using Git::Git;

private:
    SourceType getType() const override { return SourceType::Fossil; }
    void download1(const path &dir) const override;
};

struct SW_MANAGER_API Cvs : SourceUrl
{
    String tag;
    String branch;
    String revision;
    String module;

    Cvs(const String &url, const String &module, const String &tag = "", const String &branch = "", const String &revision = "");

    Cvs(const nlohmann::json &j);
    Cvs(const ptree &p);
    Cvs(const yaml &root);

    void applyVersion(const Version &v) override;

private:
    SourceType getType() const override { return SourceType::Cvs; }
    void checkUrl() const override;
    String print1() const override;
    void download1(const path &dir) const override;
    void save1(nlohmann::json &p) const override;
    void save1(yaml &root) const override;
    void save1(ptree &p) const override;
    void printKv(SourceKvMap &) const override;
};

struct Svn : SourceUrl
{
    String tag;
    String branch;
    int64_t revision = -1;

    Svn(const String &url, const String &tag = "", const String &branch = "", int64_t revision = -1);

    Svn(const nlohmann::json &j);
    Svn(const ptree &p);
    Svn(const yaml &root);

    void applyVersion(const Version &v) override;

private:
    SourceType getType() const override { return SourceType::Svn; }
    String print1() const override;
    void download1(const path &dir) const override;
    void save1(nlohmann::json &p) const override;
    void save1(yaml &root) const override;
    void save1(ptree &p) const override;
    void printKv(SourceKvMap &) const override;
};

struct SW_MANAGER_API RemoteFile : SourceUrl
{
    using SourceUrl::SourceUrl;

private:
    SourceType getType() const override { return SourceType::RemoteFile; }
    void download1(const path &dir) const override;
};

struct SW_MANAGER_API RemoteFiles : Source
{
    StringSet urls;

    RemoteFiles(const StringSet &urls);

    RemoteFiles(const nlohmann::json &j);
    RemoteFiles(const ptree &p);
    RemoteFiles(const yaml &root);

    void applyVersion(const Version &v) override;

private:
    SourceType getType() const override { return SourceType::RemoteFiles; }
    String print1() const override;
    void download1(const path &dir) const override;
    void save1(nlohmann::json &p) const override;
    void save1(yaml &root) const override;
    void save1(ptree &p) const override;
    void printKv(SourceKvMap &) const override;
};

} // inline namespace source

using SourceDirMap = std::unordered_map<SourcePtr, path>;
using SourceDirSet = std::unordered_set<SourcePtr>;

struct SourceDownloadOptions
{
    path source_dir;
    path root_dir; // root to download
    bool ignore_existing_dirs = false;
    std::chrono::seconds existing_dirs_age{ 0 };
    bool adjust_root_dir = true;
};

SW_MANAGER_API
void download(SourceDirMap &sources, const SourceDownloadOptions &opts = {});

SW_MANAGER_API
SourceDirMap download(SourceDirSet &sources, const SourceDownloadOptions &opts = {});

}

namespace std
{

template<> struct hash<::sw::SourcePtr>
{
    size_t operator()(const ::sw::SourcePtr &p) const
    {
        return hash<String>()(p->print());
    }
};

}
