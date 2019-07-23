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

sw::SourceDirMap fetch(sw::SwBuild &b)
{
    using namespace sw;

    sw::SourceDownloadOptions opts;
    opts.root_dir = fs::current_path() / SW_BINARY_DIR;
    opts.ignore_existing_dirs = true;
    opts.existing_dirs_age = std::chrono::hours(1);

    auto &i = b.addInput(fs::current_path());
    auto ts = b.swctx.getHostSettings();
    ts["driver"]["dry-run"] = "true";
    i.addSettings(ts);
    b.load();

    auto d = fs::current_path() / SW_BINARY_DIR / "src";

    SourceDirMap srcs;
    std::unordered_set<SourcePtr> sources;
    for (const auto &[pkg, tgts] : b.getTargets())
    {
        // filter out predefined targets
        if (b.swctx.getPredefinedTargets().find(pkg) != b.swctx.getPredefinedTargets().end())
            continue;
        auto tgt = tgts.getAnyTarget();
        if (!tgt)
            throw SW_RUNTIME_ERROR("Empty target");
        auto &t = *tgt;

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
    ts["driver"]["dry-run"] = "false";
    for (auto &[h, d] : srcs)
        ts["driver"]["source-dir-for-source"][h] = normalize_path(d);
    i.addSettings(ts);
    b.load();
    b.setTargetsToBuild();
    b.resolvePackages();
    b.loadPackages();
    b.prepare();

    if (build_after_fetch)
        b.execute();

    return srcs;
}

SUBCOMMAND_DECL2(fetch)
{
    auto b = swctx.createBuild();
    fetch(b);
}
