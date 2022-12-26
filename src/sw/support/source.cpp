// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>

#include "source.h"

#include <sw/support/filesystem.h>

#include <nlohmann/json.hpp>
#include <primitives/date_time.h>
#include <primitives/exceptions.h>
#include <primitives/executor.h>
#include <primitives/yaml.h>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "source");

namespace sw
{

inline namespace source
{

Git::Git(const String &url)
    : primitives::source::Git(url, "", "", "", true)
{
}

bool Git::isValid()
{
    int i = 0;
    i += !branch.empty();
    i += !tag.empty();
    i += !commit.empty();
    return i == 1;
}

std::unique_ptr<Source> load(const nlohmann::json &j)
{
    if (j.contains("git"))
        return std::make_unique<Git>(j["git"]);
    return ::primitives::source::Source::load(j);
}

} // inline namespace source

namespace support
{

detail::DownloadData::~DownloadData()
{
    if (delete_in_dtor)
        remove();
}

void detail::DownloadData::remove() const
{
    fs::remove_all(root_dir);
    fs::remove(stamp_file);
}

bool download(const std::unordered_set<SourcePtr> &sset, SourceDirMap &source_dirs, const SourceDownloadOptions &opts)
{
    std::atomic_bool downloaded = false;
    Executor e;
    Futures<void> fs;
    for (auto &src : sset) {
        fs.push_back(e.push([src = src.get(), &d = source_dirs[src->getHash()], &opts, &downloaded] {
            auto &t = d.stamp_file;
            t = d.root_dir;
            t += ".stamp";

            auto dl = [&src, &d, &t, &downloaded]() {
                downloaded = true;
                LOG_INFO(logger, "Downloading source:\n" << src->print());
                src->download(d.root_dir);
                write_file(t, timepoint2string(getUtc()));
            };

            if (!fs::exists(d.root_dir)) {
                dl();
            }
            else if (!opts.ignore_existing_dirs) {
                throw SW_RUNTIME_ERROR("Directory exists " + to_string(d.root_dir) + " for source " + src->print());
            }
            else {
                bool e = fs::exists(t);
                if (!e) {
                    fs::remove_all(d.root_dir);
                    dl();
                }
                else if (getUtc() - string2timepoint(read_file(t)) > opts.existing_dirs_age) {
                    // add src->needsRedownloading()?
                    auto g = dynamic_cast<primitives::source::Git *>(src);
                    if (g && (!g->tag.empty() || !g->commit.empty()))
                        ;
                    else {
                        if (e)
                            LOG_INFO(logger, "Download data is stale, re-downloading");
                        fs::remove_all(d.root_dir);
                        dl();
                    }
                }
            }
            d.requested_dir = d.root_dir;
            if (opts.adjust_root_dir)
                d.requested_dir /= findRootDirectory(d.requested_dir); // pass found regex or files for better root dir lookup
        }));
    }
    waitAndGet(fs);
    return downloaded;
}

SourceDirMap download(const std::unordered_set<SourcePtr> &sset, const SourceDownloadOptions &opts)
{
    SourceDirMap sources;
    for (auto &s : sset)
        sources[s->getHash()].root_dir = opts.root_dir.empty() ? get_temp_filename("dl") : (opts.root_dir / s->getHash());
    download(sset, sources, opts);
    return sources;
}

} // namespace support

} // namespace sw
