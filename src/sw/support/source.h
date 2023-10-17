// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include <primitives/source.h>

#include <primitives/executor.h>

namespace sw
{

inline namespace source
{

using primitives::source::Source;

using primitives::source::EmptySource;
using primitives::source::Hg;
using primitives::source::Mercurial;
using primitives::source::Bzr;
using primitives::source::Bazaar;
using primitives::source::Fossil;
using primitives::source::Cvs;
using primitives::source::Svn;
using primitives::source::RemoteFile;
using primitives::source::RemoteFiles;

struct SW_SUPPORT_API Git : primitives::source::Git
{
    using primitives::source::Git::Git;

    Git(const String &url);
    Git(const Git &) = default;

    bool isValid();

private:
    std::unique_ptr<Source> clone() const override { return std::make_unique<Git>(*this); }
};

/// load from current (passed) object, detects 'getString()' subobject
SW_SUPPORT_API
std::unique_ptr<Source> load(const nlohmann::json &j);

}

namespace support
{

namespace detail
{

struct SW_SUPPORT_API DownloadData
{
    path root_dir;
    path requested_dir;
    path stamp_file;

    path getRequestedDirectory() const { return requested_dir; }
    path getRealSourceJsonFile() const { return path{root_dir} += ".source.json"s; }
    void remove() const;
};

}

using SourcePtr = std::unique_ptr<Source>;
using SourceDirMap = std::unordered_map<String, detail::DownloadData>;

struct SourceDownloadOptions
{
    path root_dir; // root to download
    bool ignore_existing_dirs = false;
    std::chrono::seconds existing_dirs_age{ 0 };
    bool adjust_root_dir = true;
};

// returns true if downloaded
SW_SUPPORT_API
bool download(Executor &, const std::unordered_set<SourcePtr> &sources, SourceDirMap &source_dirs, const SourceDownloadOptions &opts = {});

SW_SUPPORT_API
SourceDirMap download(Executor &, const std::unordered_set<SourcePtr> &sources, const SourceDownloadOptions &opts = {});

} // namespace support

} // namespace sw
