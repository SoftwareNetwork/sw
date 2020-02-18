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

sw::Remote *find_remote(sw::Settings &s, const String &name);

SUBCOMMAND_DECL(upload)
{
    auto swctx = createSwContext(options);
    cli_upload(*swctx, options);
}

sw::SourcePtr createSource(OPTIONS_ARG_CONST)
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

        if (t.getInterfaceSettings()["skip_upload"] == "true")
            continue;

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

    // get spec early, so changes won't be considered
    auto inputs = swctx.addInput(fs::current_path());
    SW_CHECK(inputs.size() == 1); // for now
    auto spec = inputs[0]->getSpecification()->files.begin()->second;

    // detect from options
    bool cmdline_source_present = 0
        || !options.options_upload.source.empty()
        || !options.options_upload.git.empty()
        || !options.options_upload.hg.empty()
        || !options.options_upload.bzr.empty()
        || !options.options_upload.fossil.empty()
        || !options.options_upload.svn.empty()
        || !options.options_upload.cvs.empty()
        || !options.options_upload.remote.empty()
    ;
    if (cmdline_source_present)
    {
        if (options.options_upload.version.empty())
            throw SW_RUNTIME_ERROR("version must be present on cmd as well");
        if (options.options_upload.source.empty())
        {
#define CHECK_AND_ASSIGN(x)                     \
    else if (!options.options_upload.x.empty()) \
        options.options_upload.source = #x
            if (0);
            CHECK_AND_ASSIGN(git);
            CHECK_AND_ASSIGN(hg);
            CHECK_AND_ASSIGN(bzr);
            CHECK_AND_ASSIGN(fossil);
            CHECK_AND_ASSIGN(svn);
            CHECK_AND_ASSIGN(cvs);
            CHECK_AND_ASSIGN(remote);
#undef CHECK_AND_ASSIGN
        }
    }

    auto [sources, i] = fetch(*b, options);
    if (sources.empty())
        throw SW_RUNTIME_ERROR("Empty target sources");

    // 1)
    // to get sources, we MUST prepare loaded targets
    // otherwise not all source get uploaded
    // example:
    // t = add target()
    // t -= "1.cpp";
    // in this case no .* regexes are applied and we'll get only single file
    //
    // 2)
    // We MUST perform all steps until prepare() too!
    b->setTargetsToBuild();
    b->resolvePackages();
    b->loadPackages();
    b->prepare();

    auto m = getPackages(*b, sources);

    // dbg purposes
    for (auto &[id, d] : m)
    {
        write_file(b->getBuildDirectory() / "upload" / id.toString() += ".json", d->getString());
        auto id2 = sw::PackageId(sw::PackagePath(options.options_upload.upload_prefix) / id.getPath(), id.getVersion());
        LOG_INFO(logger, "Uploading " + id2.toString());
    }

    String script_name;
    switch (i[0]->getType())
    {
    case sw::InputType::SpecificationFile:
        script_name = i[0]->getPath().filename().string();
        break;
    default:
        SW_UNIMPLEMENTED;
    }

    if (options.options_upload.upload_dry)
    {
        LOG_INFO(logger, "Dry run. Upload was cancelled.");
        return;
    }

    // select remote
    auto &us = sw::Settings::get_user_settings();
    auto current_remote = &*us.remotes.begin();
    if (!options.options_upload.upload_remote.empty())
        current_remote = find_remote(us, options.options_upload.upload_remote);

    // send signatures (gpg)
    // -k KEY1 -k KEY2
    auto api = current_remote->getApi();
    api->addVersion(options.options_upload.upload_prefix, m, script_name, spec);
}
