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

#include <sw/core/build.h>
#include <sw/core/input.h>

DEFINE_SUBCOMMAND(fetch, "Fetch sources.");

::cl::opt<bool> build_after_fetch("build", ::cl::desc("Build after fetch"), ::cl::sub(subcommand_fetch));

SUBCOMMAND_DECL(fetch)
{
    auto swctx = createSwContext();
    cli_fetch(*swctx);
}

decltype(auto) getInput(sw::SwBuild &b)
{
    return b.getContext().addInput(fs::current_path());
}

auto getSources(sw::SwContext &swctx)
{
    auto b1 = swctx.createBuild();
    auto &b = *b1;

    auto ts = createSettings(b.getContext());
    ts["driver"]["dry-run"] = "true";

    auto &ii = getInput(b);
    sw::InputWithSettings i(ii);
    i.addSettings(ts);
    b.addInput(i);
    b.load();

    auto d = fs::current_path() / SW_BINARY_DIR / "src";

    sw::SourceDirMap srcs;
    std::unordered_set<sw::SourcePtr> sources;
    for (const auto &[pkg, tgts] : b.getTargets())
    {
        // filter out predefined targets
        if (b.getContext().getPredefinedTargets().find(pkg) != b.getContext().getPredefinedTargets().end())
            continue;
        auto tgt = tgts.getAnyTarget();
        if (!tgt)
            throw SW_RUNTIME_ERROR("Empty target");
        auto &t = *tgt;

        auto &tgts2 = tgts;
        SCOPE_EXIT
        {
            // and clear targets
            tgts2.clear();
        };

        auto s = t.getSource().clone(); // make a copy!
        s->applyVersion(pkg.getVersion());
        if (srcs.find(s->getHash()) != srcs.end())
            continue;
        srcs[s->getHash()] = d / s->getHash();
        sources.emplace(std::move(s));
    }

    sw::SourceDownloadOptions opts;
    opts.root_dir = fs::current_path() / SW_BINARY_DIR;
    opts.ignore_existing_dirs = true;
    opts.existing_dirs_age = std::chrono::hours(1);

    if (download(sources, srcs, opts))
    {
        // clear patch dir to make changes to files again
        fs::remove_all(d.parent_path() / "patch");
    }
    return srcs;
}

std::pair<sw::SourceDirMap, const sw::Input &> fetch(sw::SwBuild &b)
{
    auto srcs = getSources(b.getContext());

    auto ts = createSettings(b.getContext());
    for (auto &[h, d] : srcs)
        ts["driver"]["source-dir-for-source"][h] = normalize_path(d);

    auto &ii = getInput(b);
    sw::InputWithSettings i(ii);
    i.addSettings(ts);
    b.addInput(i);
    b.overrideBuildState(sw::BuildState::NotStarted);
    b.load();
    b.setTargetsToBuild();
    b.resolvePackages();
    b.loadPackages();
    b.prepare();

    if (build_after_fetch)
        b.execute();

    return { srcs, ii };
}

std::pair<sw::SourceDirMap, const sw::Input &> fetch(sw::SwContext &swctx)
{
    return fetch(*swctx.createBuild());
}

SUBCOMMAND_DECL2(fetch)
{
    fetch(swctx);
}
