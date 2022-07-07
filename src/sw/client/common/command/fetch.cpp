// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2017-2019 Egor Pugin <egor.pugin@gmail.com>

#include "../commands.h"

#include <nlohmann/json.hpp>
#include <primitives/yaml.h>
#include <sw/core/build.h>
#include <sw/core/input.h>

// sometimes we do not want
//  sw build --fetch
// but just
//  sw fetch

static decltype(auto) getInput(sw::SwBuild &b)
{
    return b.addInput(fs::current_path());
}

static sw::support::SourcePtr createSource(const Options &options)
{
    sw::support::SourcePtr s;
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

    if (!options.options_upload.version.empty())
        s->applyVersion(options.options_upload.version);
    return s;
}

static sw::support::SourceDirMap getSources(auto &&ex, const path &bdir, const std::unordered_set<sw::support::SourcePtr> &sources, sw::support::SourceDirMap &srcs)
{
    sw::support::SourceDownloadOptions opts;
    opts.ignore_existing_dirs = true;
    opts.existing_dirs_age = std::chrono::hours(1);

    if (download(ex, sources, srcs, opts))
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
static sw::support::SourceDirMap getSources(SwClientContext &swctx)
{
    auto b1 = swctx.createBuild();
    auto &b = *b1;

    auto ts = swctx.createInitialSettings();
    ts["driver"]["dry-run"] = "true"; // only used to get sources

    auto inputs = getInput(b);
    for (auto &ii : inputs)
    {
        sw::InputWithSettings i(ii);
        i.addSettings(ts);
        b.addInput(i);
    }
    b.loadInputs();
    b.setTargetsToBuild();

    auto d = get_source_dir(b.getBuildDirectory());

    sw::support::SourceDirMap srcs;
    std::unordered_set<sw::support::SourcePtr> sources;
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

    return getSources(*swctx.getContext().executor, b.getBuildDirectory(), sources, srcs);
}

// get sources extracted from options
static sw::support::SourceDirMap getSources(auto &&ex, const path &bdir, const Options &options)
{
    auto s = createSource(options);
    sw::support::SourceDirMap srcs;
    std::unordered_set<sw::support::SourcePtr> sources;
    srcs[s->getHash()].root_dir = get_source_dir(bdir) / s->getHash();
    sources.emplace(std::move(s));
    return getSources(ex, bdir, sources, srcs);
}

std::pair<sw::support::SourceDirMap, std::vector<sw::BuildInput>> SwClientContext::fetch(sw::SwBuild &b)
{
    auto srcs = getOptions().options_upload.source.empty()
        ? getSources(*this) // from config
        : getSources(*getContext().executor, b.getBuildDirectory(), getOptions()); // from cmd

    auto tss = createSettings();
    for (auto &ts : tss)
    {
        for (auto &[h, d] : srcs)
        {
            ts["driver"]["source-dir-for-source"][h] = to_string(normalize_path(d.getRequestedDirectory()));
            if (!getOptions().options_upload.source.empty())
            {
                // TODO: if version is empty, load it from config
                nlohmann::json j;
                createSource(getOptions())->save(j);
                ts["driver"]["force-source"] = j.dump();
            }
        }
    }

    auto inputs = getInput(b);
    for (auto &ii : inputs)
    {
        sw::InputWithSettings i(ii);
        for (auto &ts : tss)
            i.addSettings(ts);
        b.addInput(i);
    }
    b.loadInputs();

    if (getOptions().options_fetch.build_after_fetch)
        b.build();

    return { srcs, inputs };
}

std::pair<sw::support::SourceDirMap, std::vector<sw::BuildInput>> SwClientContext::fetch()
{
    return fetch(*createBuild());
}

SUBCOMMAND_DECL(fetch)
{
    fetch();
}
