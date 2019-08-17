// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "source.h"

#include <sw/support/filesystem.h>

#include <primitives/date_time.h>
#include <primitives/exceptions.h>
#include <primitives/executor.h>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "source");

namespace sw
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

bool download(const std::unordered_set<SourcePtr> &sset, SourceDirMap &source_dirs, const SourceDownloadOptions &opts)
{
    std::atomic_bool downloaded = false;
    auto &e = getExecutor();
    Futures<void> fs;
    for (auto &src : sset)
    {
        fs.push_back(e.push([src = src.get(), &d = source_dirs[src->getHash()], &opts, &downloaded]
        {
            path t = d;
            t += ".stamp";

            auto dl = [&src, d, &t, &downloaded]()
            {
                downloaded = true;
                LOG_INFO(logger, "Downloading source:\n" << src->print());
                src->download(d);
                write_file(t, timepoint2string(getUtc()));
            };

            if (!fs::exists(d))
            {
                dl();
            }
            else if (!opts.ignore_existing_dirs)
            {
                throw SW_RUNTIME_ERROR("Directory exists " + normalize_path(d) + " for source " + src->print());
            }
            else
            {
                bool e = fs::exists(t);
                if (!e || getUtc() - string2timepoint(read_file(t)) > opts.existing_dirs_age)
                {
                    // add src->needsRedownloading()?
                    auto g = dynamic_cast<primitives::source::Git *>(src);
                    if (g && (!g->tag.empty() || !g->commit.empty()))
                        ;
                    else
                    {
                        if (e)
                            LOG_INFO(logger, "Download data is stale, re-downloading");
                        fs::remove_all(d);
                        dl();
                    }
                }
            }
            if (opts.adjust_root_dir)
                d = d / findRootDirectory(d); // pass found regex or files for better root dir lookup
        }));
    }
    waitAndGet(fs);
    return downloaded;
}

SourceDirMap download(const std::unordered_set<SourcePtr> &sset, const SourceDownloadOptions &opts)
{
    SourceDirMap sources;
    for (auto &s : sset)
        sources[s->getHash()] = opts.root_dir.empty() ? get_temp_filename("dl") : (opts.root_dir / s->getHash());
    download(sset, sources, opts);
    return sources;
}

}
