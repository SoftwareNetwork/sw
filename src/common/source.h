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

#pragma once

#include "cppan_string.h"
#include "filesystem.h"
#include "property_tree.h"
#include "version.h"

#include <primitives/stdcompat/variant.h>

#include <set>

namespace YAML { class Node; }
using yaml = YAML::Node;

struct SourceUrl
{
    String url;

    SourceUrl() = default;
    SourceUrl(const yaml &root, const String &name);

    bool empty() const { return url.empty(); }
    bool isValid(const String &name, String *error = nullptr) const;
    bool isValidUrl() const;
    bool load(const ptree &p);
    bool save(ptree &p) const;
    void save(yaml &root, const String &name) const;
    String print() const;
    void applyVersion(const Version &v);
    void loadVersion(Version &v) {}

protected:
    template <typename ... Args>
    bool checkValid(const String &name, String *error, Args && ... args) const;
};

struct Git : SourceUrl
{
    String tag;
    String branch;
    String commit;

    Git() = default;
    Git(const yaml &root, const String &name = Git::getString());

    void download() const;
    bool isValid(String *error = nullptr) const;
    bool load(const ptree &p);
    bool save(ptree &p) const;
    void save(yaml &root, const String &name = Git::getString()) const;
    String print() const;
    String printCpp() const;
    void applyVersion(const Version &v);
    void loadVersion(Version &v);

    bool operator==(const Git &rhs) const
    {
        return std::tie(url, tag, branch, commit) == std::tie(rhs.url, rhs.tag, rhs.branch, rhs.commit);
    }

    static String getString() { return "git"; }
};

struct Hg : Git
{
    int64_t revision = -1;

    Hg() = default;
    Hg(const yaml &root, const String &name = Hg::getString());

    void download() const;
    bool isValid(String *error = nullptr) const;
    bool load(const ptree &p);
    bool save(ptree &p) const;
    void save(yaml &root, const String &name = Hg::getString()) const;
    String print() const;
    String printCpp() const;
    void loadVersion(Version &v);

    bool operator==(const Hg &rhs) const
    {
        return std::tie(url, tag, branch, commit, revision) == std::tie(rhs.url, rhs.tag, rhs.branch, rhs.commit, rhs.revision);
    }

    static String getString() { return "hg"; }
};

struct Bzr : SourceUrl
{
    String tag;
    int64_t revision = -1;

    Bzr() = default;
    Bzr(const yaml &root, const String &name = Bzr::getString());

    void download() const;
    bool isValid(String *error = nullptr) const;
    bool load(const ptree &p);
    bool save(ptree &p) const;
    void save(yaml &root, const String &name = Bzr::getString()) const;
    String print() const;
    String printCpp() const;
    void loadVersion(Version &v);

    bool operator==(const Bzr &rhs) const
    {
        return std::tie(url, tag, revision) == std::tie(rhs.url, rhs.tag, rhs.revision);
    }

    static String getString() { return "bzr"; }
};

struct Fossil : Git
{
    Fossil() = default;
    Fossil(const yaml &root, const String &name = Fossil::getString());

    void download() const;
    using Git::save;
    void save(yaml &root, const String &name = Fossil::getString()) const;
    void loadVersion(Version &v);

    bool operator==(const Fossil &rhs) const
    {
        return std::tie(url, tag, branch, commit) == std::tie(rhs.url, rhs.tag, rhs.branch, rhs.commit);
    }

    static String getString() { return "fossil"; }
};

struct Cvs : SourceUrl
{
    String tag;
    String branch;
    String revision;
    String module;

    Cvs() = default;
    Cvs(const yaml &root, const String &name = Cvs::getString());

    void download() const;
    bool isValid(String *error = nullptr) const;
    bool isValidUrl() const;
    bool load(const ptree &p);
    bool save(ptree &p) const;
    void save(yaml &root, const String &name = Cvs::getString()) const;
    String print() const;
    String printCpp() const;
    void loadVersion(Version &v);

    bool operator==(const Cvs &rhs) const
    {
        return std::tie(url, tag, branch, revision, module) == std::tie(rhs.url, rhs.tag, rhs.branch, rhs.revision, rhs.module);
    }

    static String getString() { return "cvs"; }
};

struct RemoteFile : SourceUrl
{
    RemoteFile() = default;
    RemoteFile(const yaml &root, const String &name = RemoteFile::getString());

    void download() const;
    using SourceUrl::save;
    void save(yaml &root, const String &name = RemoteFile::getString()) const;
    String printCpp() const;
    void applyVersion(const Version &v);

    bool operator==(const RemoteFile &rhs) const
    {
        return url == rhs.url;
    }

    static String getString() { return "remote"; }
};

struct RemoteFiles
{
    StringSet urls;

    RemoteFiles() = default;
    RemoteFiles(const yaml &root, const String &name = RemoteFiles::getString());

    void download() const;
    bool empty() const { return urls.empty(); }
    bool isValidUrl() const;
    bool load(const ptree &p);
    bool save(ptree &p) const;
    void save(yaml &root, const String &name = RemoteFiles::getString()) const;
    String print() const;
    String printCpp() const;
    void applyVersion(const Version &v);
    void loadVersion(Version &v) {}

    bool operator==(const RemoteFiles &rhs) const
    {
        return urls == rhs.urls;
    }

    static String getString() { return "files"; }
};

// TODO: add: svn, cvs, darcs, p4
// do not add local files
#define SOURCE_TYPES(f,d) \
    f(Git) d \
    f(Hg) d \
    f(Bzr) d \
    f(Fossil) d \
    f(Cvs) d \
    f(RemoteFile) d \
    f(RemoteFiles)

#define DELIM_COMMA ,
#define DELIM_SEMICOLON ;
#define SOURCE_TYPES_EMPTY(x) x
using Source = variant<SOURCE_TYPES(SOURCE_TYPES_EMPTY, DELIM_COMMA)>;
#undef SOURCE_TYPES_EMPTY

void download(const Source &source, int64_t max_file_size = 0);
bool load_source(const yaml &root, Source &source);
Source load_source(const ptree &p);
void save_source(yaml &root, const Source &source);
void save_source(ptree &p, const Source &source);
String print_source(const Source &source);
String print_source_cpp(const Source &source);
void applyVersionToUrl(Source &source, const Version &v);

bool isValidSourceUrl(const Source &source);
