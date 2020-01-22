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

#include <sw/core/driver.h>
#include <sw/core/input.h>
#include <sw/manager/api.h>
#include <sw/manager/settings.h>

#include <nlohmann/json.hpp>
#include <primitives/pack.h>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "upload");

DEFINE_SUBCOMMAND(upload, "Upload packages.");

extern ::cl::opt<String> build_arg_update;

static ::cl::opt<String> upload_remote(::cl::Positional, ::cl::desc("Remote name"), ::cl::sub(subcommand_upload));
String gUploadPrefix;
static ::cl::opt<String, true> upload_prefix(::cl::Positional, ::cl::desc("Prefix path"), ::cl::sub(subcommand_upload),
    ::cl::Required, ::cl::location(gUploadPrefix));

sw::Remote *find_remote(sw::Settings &s, const String &name);

SUBCOMMAND_DECL(upload)
{
    auto swctx = createSwContext();
    cli_upload(*swctx);
}

sw::PackageDescriptionMap getPackages(const sw::SwBuild &b, const sw::SourceDirMap &sources)
{
    using namespace sw;

    PackageDescriptionMap m;
    for (auto &[pkg, tgts] : b.getTargets())
    {
        // deps
        if (pkg.getPath().isAbsolute())
            continue;

        if (tgts.empty())
            throw SW_RUNTIME_ERROR("Empty targets");

        auto &t = **tgts.begin();
        nlohmann::json j;

        // source, version, path
        t.getSource().save(j["source"]);
        j["version"] = pkg.getVersion().toString();
        j["path"] = pkg.getPath().toString();

        // find root dir
        path rd;
        if (!sources.empty())
        {
            auto src = t.getSource().clone(); // copy
            src->applyVersion(pkg.getVersion());
            auto si = sources.find(src->getHash());
            if (si == sources.end())
                throw SW_RUNTIME_ERROR("no such source");
            rd = si->second.getRequestedDirectory();
        }
        j["root_dir"] = normalize_path(rd);

        // double check files (normalize them)
        Files files;
        for (auto &f : t.getSourceFiles())
            files.insert(f.lexically_normal());

        // we put files under SW_SDIR_NAME to keep space near it
        // e.g. for patch dir or other dirs (server provided files)
        // we might unpack to other dir, but server could push service files in neighbor dirs like gpg keys etc
        nlohmann::json jm;
        auto files_map1 = primitives::pack::prepare_files(files, rd.lexically_normal());
        for (const auto &[f1, f2] : files_map1)
        {
            nlohmann::json jf;
            jf["from"] = normalize_path(f1);
            jf["to"] = normalize_path(f2);
            j["files"].push_back(jf);
        }

        // deps
        for (auto &d : t.getDependencies())
        {
            // filter out predefined targets
            if (b.getContext().getPredefinedTargets().find(d->getUnresolvedPackage().ppath) != b.getContext().getPredefinedTargets().end(d->getUnresolvedPackage().ppath))
                continue;

            nlohmann::json jd;
            jd["path"] = d->getUnresolvedPackage().ppath.toString();
            jd["range"] = d->getUnresolvedPackage().range.toString();
            j["dependencies"].push_back(jd);
        }

        auto s = j.dump();
        m[pkg] = std::make_unique<JsonPackageDescription>(s);
    }
    return m;
}

SUBCOMMAND_DECL2(upload)
{
    auto b = swctx.createBuild();
    auto [sources, i] = fetch(*b);
    if (sources.empty())
        throw SW_RUNTIME_ERROR("Empty target sources");

    // to get sources, we MUST prepare loaded targets
    // otherwise not all source get uploaded
    // example:
    // t = add target()
    // t -= "1.cpp";
    // in this case no .* regexes are applied and we'll get only single file
    b->overrideBuildState(sw::BuildState::PackagesLoaded);
    b->prepare();

    auto m = getPackages(*b, sources);

    // dbg purposes
    for (auto &[id, d] : m)
    {
        write_file(b->getBuildDirectory() / "upload" / id.toString() += ".json", d->getString());
        auto id2 = sw::PackageId(sw::PackagePath(upload_prefix) / id.getPath(), id.getVersion());
        LOG_INFO(logger, "Uploading " + id2.toString());
    }

    // select remote first
    auto &us = sw::Settings::get_user_settings();
    auto current_remote = &*us.remotes.begin();
    if (!upload_remote.empty())
        current_remote = find_remote(us, upload_remote);

    String script_name;
    switch (i.getType())
    {
    case sw::InputType::SpecificationFile:
        script_name = i.getPath().filename().string();
        break;
    default:
        SW_UNIMPLEMENTED;
    }

    // send signatures (gpg)
    // -k KEY1 -k KEY2
    auto api = current_remote->getApi();
    api->addVersion(gUploadPrefix, m, script_name, i.getSpecification()->files.begin()->second);
}
