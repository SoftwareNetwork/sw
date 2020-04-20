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

#include "../commands.h"

#include <nlohmann/json.hpp>
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

static sw::SourcePtr createSource(const Options &options)
{
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

    if (!options.options_upload.version.empty())
        s->applyVersion(options.options_upload.version);
    return s;
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
static sw::SourceDirMap getSources(SwClientContext &swctx)
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
static sw::SourceDirMap getSources(const path &bdir, const Options &options)
{
    auto s = createSource(options);
    sw::SourceDirMap srcs;
    std::unordered_set<sw::SourcePtr> sources;
    srcs[s->getHash()].root_dir = get_source_dir(bdir) / s->getHash();
    sources.emplace(std::move(s));
    return getSources(bdir, sources, srcs);
}

std::pair<sw::SourceDirMap, std::vector<sw::BuildInput>> SwClientContext::fetch(sw::SwBuild &b)
{
    auto srcs = getOptions().options_upload.source.empty()
        ? getSources(*this) // from config
        : getSources(b.getBuildDirectory(), getOptions()); // from cmd

    auto tss = createSettings();
    for (auto &ts : tss)
    {
        for (auto &[h, d] : srcs)
        {
            ts["driver"]["source-dir-for-source"][h] = normalize_path(d.getRequestedDirectory());
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

std::pair<sw::SourceDirMap, std::vector<sw::BuildInput>> SwClientContext::fetch()
{
    return fetch(*createBuild());
}

SUBCOMMAND_DECL(fetch)
{
    fetch();
}
