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

#include <sw/core/build.h>
#include <sw/core/input.h>

// sometimes we do not want
//  sw build --fetch
// but just
//  sw fetch

SUBCOMMAND_DECL(fetch)
{
    auto swctx = createSwContext(options);
    cli_fetch(*swctx, options);
}

static decltype(auto) getInput(sw::SwBuild &b)
{
    return b.getContext().addInput(fs::current_path());
}

static sw::SourceDirMap getSources(const path &bdir, const std::unordered_set<sw::SourcePtr> &sources, sw::SourceDirMap &srcs)
{
    sw::SourceDownloadOptions opts;
    opts.ignore_existing_dirs = true;
    opts.existing_dirs_age = std::chrono::hours(1);

    if (download(sources, srcs, opts))
    {
        // clear patch dir to make changes to files again
        fs::remove_all(bdir / "patch");
    }
    return srcs;
}

static auto get_source_dir(const path &bdir)
{
    return bdir / "src";
}

// get sources extracted from config
static sw::SourceDirMap getSources(sw::SwContext &swctx, OPTIONS_ARG_CONST)
{
    auto b1 = createBuild(swctx, options);
    auto &b = *b1;

    auto ts = createInitialSettings(swctx);
    ts["driver"]["dry-run"] = "true"; // only used to get sources
    //ts["driver"].useInHash(false);
    //ts["driver"].ignoreInComparison(true);

    auto &ii = getInput(b);
    sw::InputWithSettings i(ii);
    i.addSettings(ts);
    b.addInput(i);
    b.loadInputs();
    b.setTargetsToBuild();

    auto d = get_source_dir(b.getBuildDirectory());

    sw::SourceDirMap srcs;
    std::unordered_set<sw::SourcePtr> sources;
    for (const auto &[pkg, tgts] : b.getTargetsToBuild())
    {
        if (tgts.empty())
            throw SW_RUNTIME_ERROR("Empty targets");

        auto &t = **tgts.begin();
        auto s = t.getSource().clone(); // make a copy!
        s->applyVersion(pkg.getVersion());
        if (srcs.find(s->getHash()) != srcs.end())
            continue;
        srcs[s->getHash()].root_dir = d / s->getHash();
        sources.emplace(std::move(s));
    }

    return getSources(b.getBuildDirectory(), sources, srcs);
}

// get sources extracted from options
static sw::SourceDirMap getSources(const path &bdir, OPTIONS_ARG_CONST)
{
    auto d = get_source_dir(bdir);

    sw::SourceDirMap srcs;
    std::unordered_set<sw::SourcePtr> sources;

    sw::SourcePtr s;
    if (0);
    else if (options.options_upload.source == "git")
    {
        s = std::make_unique<sw::Git>(
            options.options_upload.git,
            options.options_upload.tag,
            options.options_upload.branch,
            options.options_upload.commit
            );
    }
    else if (options.options_upload.source == "hg")
    {
        s = std::make_unique<sw::Hg>(
            options.options_upload.hg,
            options.options_upload.tag,
            options.options_upload.branch,
            options.options_upload.commit,
            std::stoll(options.options_upload.revision)
            );
    }
    else if (options.options_upload.source == "fossil")
    {
        s = std::make_unique<sw::Fossil>(
            options.options_upload.fossil,
            options.options_upload.tag,
            options.options_upload.branch,
            options.options_upload.commit
            );
    }
    else if (options.options_upload.source == "bzr")
    {
        s = std::make_unique<sw::Bazaar>(
            options.options_upload.bzr,
            options.options_upload.tag,
            std::stoll(options.options_upload.revision)
            );
    }
    else if (options.options_upload.source == "cvs")
    {
        s = std::make_unique<sw::Cvs>(
            options.options_upload.cvs,
            options.options_upload.module,
            options.options_upload.tag,
            options.options_upload.branch,
            options.options_upload.revision
            );
    }
    else if (options.options_upload.source == "svn")
    {
        s = std::make_unique<sw::Svn>(
            options.options_upload.svn,
            options.options_upload.tag,
            options.options_upload.branch,
            std::stoll(options.options_upload.revision)
            );
    }
    else if (options.options_upload.source == "remote")
    {
        s = std::make_unique<sw::RemoteFile>(
            options.options_upload.remote[0]
            );
    }
    else if (options.options_upload.source == "remotes")
    {
        s = std::make_unique<sw::RemoteFiles>(
            StringSet(options.options_upload.remote.begin(), options.options_upload.remote.end())
            );
    }

    srcs[s->getHash()].root_dir = d / s->getHash();
    sources.emplace(std::move(s));

    return getSources(bdir, sources, srcs);
}

std::pair<sw::SourceDirMap, const sw::Input &> fetch(sw::SwBuild &b, OPTIONS_ARG_CONST)
{
    auto srcs = options.options_upload.source.empty()
        ? getSources(b.getContext(), options) // from config
        : getSources(b.getBuildDirectory(), options); // from cmd

    auto tss = createSettings(b.getContext(), options);
    for (auto &ts : tss)
    {
        for (auto &[h, d] : srcs)
            ts["driver"]["source-dir-for-source"][h] = normalize_path(d.getRequestedDirectory());
        //ts["driver"].useInHash(false);
        //ts["driver"].ignoreInComparison(true);
    }

    auto &ii = getInput(b);
    sw::InputWithSettings i(ii);
    for (auto &ts : tss)
        i.addSettings(ts);
    b.addInput(i);
    b.loadInputs(); // download occurs here

    if (options.options_fetch.build_after_fetch)
        b.build();

    return { srcs, ii };
}

std::pair<sw::SourceDirMap, const sw::Input &> fetch(sw::SwContext &swctx, OPTIONS_ARG_CONST)
{
    return fetch(*createBuild(swctx, options), options);
}

SUBCOMMAND_DECL2(fetch)
{
    fetch(swctx, options);
}
