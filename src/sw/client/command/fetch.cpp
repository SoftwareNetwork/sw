/*
 * SW - Build System and Package Manager
 * Copyright (C) 2017-2019 Egor Pugin
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "commands.h"
#include "../build.h"

#include <sw/driver/build.h>

::cl::opt<bool> build_after_fetch("build", ::cl::desc("Build after fetch"), ::cl::sub(subcommand_fetch));

SUBCOMMAND_DECL(fetch)
{
    auto swctx = createSwContext();
    cli_fetch(*swctx);
}

/*static auto fetch1(const SwContext &swctx, const path &fn, const FetchOptions &opts)
{
    auto d = fn.parent_path() / SW_BINARY_DIR / "src";

    SourceDirMap srcs_old;
    if (opts.parallel)
    {
        bool pp = true; // postpone once!
        while (1)
        {
            auto b = std::make_unique<Build>(swctx);
            b->NamePrefix = opts.name_prefix;
            b->perform_checks = !pp && !opts.dry_run;
            b->DryRun = pp;
            b->source_dirs_by_source = srcs_old;
            b->prefix_source_dir = opts.source_dir;
            if (!pp)
                b->fetch_dir = d;
            b->load(fn);

            SourceDirMap srcs;
            std::unordered_set<SourcePtr> sources;
            for (const auto &[pkg, tgts] : b->getChildren())
            {
                auto &t = tgts.begin()->second;
                if (t->sw_provided)
                    continue;
                if (t->skip)
                    continue;
                auto s = t->getSource().clone(); // make a copy!
                s->applyVersion(pkg.getVersion());
                if (srcs.find(s->getHash()) != srcs.end())
                    continue;
                srcs[s->getHash()] = d / s->getHash();
                sources.emplace(std::move(s));
            }

            // src_old has correct root dirs
            if (srcs.size() == srcs_old.size())
            {
                if (srcs.size() == 0)
                    throw SW_RUNTIME_ERROR("no sources found");

                // reset
                b->fetch_dir.clear();
                b->fetch_info.sources = srcs_old;

                return b;
            }

            // with this, we only have two iterations
            // This is a limitation, but on the other hand handling of this
            // become too complex for now.
            // For other cases uses non-parallel mode.
            pp = false;

            download(sources, srcs, opts);
            srcs_old = srcs;
        }
    }
    else
    {
        throw SW_RUNTIME_ERROR("not implemented, try to restore (check and enable) this functionality");

        auto b = std::make_unique<Build>(swctx);
        b->NamePrefix = opts.name_prefix;
        b->perform_checks = !opts.dry_run;
        b->DryRun = opts.dry_run;
        b->fetch_dir = d;
        b->prefix_source_dir = opts.source_dir;
        b->load(fn);

        // reset
        b->fetch_dir.clear();
        b->fetch_info.sources = srcs_old;

        return b;
    }
}

std::unique_ptr<Build> fetch_and_load(const SwContext &swctx, const path &file_or_dir, const FetchOptions &opts)
{
    auto f = resolveConfig(file_or_dir);
    if (!f || !Build::isFrontendConfigFilename(f.value()))
        throw SW_RUNTIME_ERROR("no config found");

    auto b = fetch1(swctx, f.value(), opts);

    if (opts.parallel)
    {
        for (const auto &[pkg, tgts] : b->getChildren())
        {
            auto &t = tgts.begin()->second;
            if (t->sw_provided)
                continue;
            if (t->skip)
                continue;
            auto s = t->getSource().clone(); // make a copy!
            s->applyVersion(pkg.version);
            if (opts.apply_version_to_source)
            {
                SW_UNIMPLEMENTED;
                //applyVersionToUrl(t->source, pkg.version);
            }
        }
    }
    b->prepareStep();
    return std::move(b);
}*/

SUBCOMMAND_DECL2(fetch)
{
    using namespace sw;

    sw::FetchOptions opts;
    //opts.name_prefix = upload_prefix;
    opts.dry_run = !build_after_fetch;
    opts.root_dir = fs::current_path() / SW_BINARY_DIR;
    opts.ignore_existing_dirs = true;
    opts.existing_dirs_age = std::chrono::hours(1);
    //opts.apply_version_to_source = true;

    auto &i = swctx.addInput(fs::current_path());
    auto ts = toTargetSettings(swctx.getHostOs());
    //ts["name-prefix"] = "true";
    ts["dry-run"] = "true";
    i.addSettings(ts);
    swctx.load();

    auto d = fs::current_path() / SW_BINARY_DIR / "src";

    SourceDirMap srcs;
    std::unordered_set<SourcePtr> sources;
    for (const auto &[pkg, tgts] : swctx.getTargets())
    {
        auto &t = **tgts.begin();
        if (!t.isReal())
            continue;

        SCOPE_EXIT
        {
            // and clear targets
            tgts.clear();
        };

        auto s = t.getSource().clone(); // make a copy!
        s->applyVersion(pkg.getVersion());
        if (srcs.find(s->getHash()) != srcs.end())
            continue;
        srcs[s->getHash()] = d / s->getHash();
        sources.emplace(std::move(s));
    }

    download(sources, srcs, opts);

    i.clearSettings();
    ts["dry-run"] = "false";
    for (auto &[h, d] : srcs)
        ts["source-dir-for-source"][h] = normalize_path(d);
    i.addSettings(ts);
    swctx.load();

    if (build_after_fetch)
        swctx.execute();
}
