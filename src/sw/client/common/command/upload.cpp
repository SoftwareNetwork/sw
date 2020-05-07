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

#include <sw/core/input.h>
#include <sw/core/specification.h>
#include <sw/manager/api.h>
#include <sw/manager/settings.h>
#include <sw/manager/remote.h>

#include <nlohmann/json.hpp>
#include <primitives/pack.h>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "upload");

sw::Remote *find_remote(sw::Settings &s, const String &name);

sw::PackageDescriptionMap getPackages(const sw::SwBuild &b, const sw::SourceDirMap &sources, std::map<const sw::Input*, std::vector<sw::PackageId>> *iv)
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

        // unique deps
        std::set<UnresolvedPackage> upkgs;
        for (auto &d : t.getDependencies())
        {
            // filter out predefined targets
            if (b.getContext().getPredefinedTargets().find(d->getUnresolvedPackage().ppath) != b.getContext().getPredefinedTargets().end(d->getUnresolvedPackage().ppath))
                continue;
            upkgs.insert(d->getUnresolvedPackage());
        }
        for (auto &u : upkgs)
        {
            nlohmann::json jd;
            jd["path"] = u.getPath().toString();
            jd["range"] = u.getRange().toString();
            j["dependencies"].push_back(jd);
        }

        auto s = j.dump();
        m[pkg] = std::make_unique<JsonPackageDescription>(s);
        if (iv)
            (*iv)[&tgts.getInput().getInput()].push_back(pkg);
    }
    return m;
}

static void input_check(const sw::Specification &spec)
{
    // single file for now
    SW_CHECK(spec.files.getData().size() == 1);
    // do not allow dirs for now
    SW_CHECK(spec.dir.empty());
}

SUBCOMMAND_DECL(upload)
{
    auto b = createBuild();

    // get spec early, so changes won't be noticed
    // do not move to the bottom
    auto inputs = b->getContext().detectInputs(fs::current_path());
    if (inputs.size() > 1)
        LOG_INFO(logger, "Multiple inputs detected:");
    std::unordered_map<size_t, sw::Input *> input_map;
    for (const auto &[idx, i] : enumerate(inputs))
    {
        auto &spec = i->getSpecification();
        input_check(spec);

        if (inputs.size() > 1)
            LOG_INFO(logger, "Input #" << idx << ": " << spec.files.getData().begin()->first);

        for (auto &[_, f] : spec.files.getData())
            f.read();
        input_map[i->getHash()] = i.get();
    }

    // detect from options
    bool cmdline_source_present = 0
        || !getOptions().options_upload.source.empty()
        || !getOptions().options_upload.git.empty()
        || !getOptions().options_upload.hg.empty()
        || !getOptions().options_upload.bzr.empty()
        || !getOptions().options_upload.fossil.empty()
        || !getOptions().options_upload.svn.empty()
        || !getOptions().options_upload.cvs.empty()
        || !getOptions().options_upload.remote.empty()
    ;
    if (cmdline_source_present)
    {
        if (getOptions().options_upload.version.empty())
            throw SW_RUNTIME_ERROR("version must be present on cmd as well");
        if (getOptions().options_upload.source.empty())
        {
#define CHECK_AND_ASSIGN(x)                     \
    else if (!getOptions().options_upload.x.empty()) \
        getOptions().options_upload.source = #x
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

    auto [sources, _] = fetch(*b);
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

    std::map<const sw::Input *, std::vector<sw::PackageId>> iv;
    auto m = getPackages(*b, sources, &iv);

    // dbg purposes
    for (auto &[id, d] : m)
    {
        write_file(b->getBuildDirectory() / "upload" / id.toString() += ".json", d->getString());
        auto id2 = sw::PackageId(sw::PackagePath(getOptions().options_upload.upload_prefix) / id.getPath(), id.getVersion());
        LOG_INFO(logger, "Uploading " + id2.toString());
    }

    if (getOptions().options_upload.upload_dry)
    {
        LOG_INFO(logger, "Dry run. Upload was cancelled.");
        return;
    }

    // select remote
    auto &us = sw::Settings::get_user_settings();
    auto current_remote = us.getRemotes().begin()->get();
    if (!getOptions().options_upload.upload_remote.empty())
        current_remote = find_remote(us, getOptions().options_upload.upload_remote);

    for (auto &[i, pkgs] : iv)
    {
        auto i2 = input_map[i->getHash()];
        SW_CHECK(i2);
        auto &spec = i2->getSpecification();

        // select this input packages
        decltype(m) m2;
        for (auto &p : pkgs)
        {
            // move only existing packages, do not create new
            if (m.find(p) != m.end())
                m2[p] = std::move(m[p]);
        }

        // send signatures (gpg etc.)?
        // -k KEY1 -k KEY2
        auto api = current_remote->getApi();
        api->addVersion(getOptions().options_upload.upload_prefix, m2, spec.files);
    }
}
