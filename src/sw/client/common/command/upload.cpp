// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2017-2019 Egor Pugin <egor.pugin@gmail.com>

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

sw::Remote &find_remote(sw::Settings &s, const String &name);

sw::PackageDescriptionMap getPackages(const sw::SwBuild &b, const sw::support::SourceDirMap &sources, std::map<const sw::Input*, std::vector<sw::PackageId>> *iv)
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

        auto d = std::make_unique<PackageDescription>(pkg, "todo-driver"s);
        d->source = t.getSource().clone();

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

        // double check files (normalize them)
        FilesSorted files;
        for (auto &[f, tf] : t.getFiles(
            //StorageFileType::SourceArchive
        ))
        {
            if (tf.isGenerated())
                continue;
            files.insert(f.lexically_normal());
        }

        // we put files under SW_SDIR_NAME to keep space near it
        // e.g. for patch dir or other dirs (server provided files)
        // we might unpack to other dir, but server could push service files in neighbor dirs like gpg keys etc
        auto files_map1 = primitives::pack::prepare_files(files, rd.lexically_normal());
        for (const auto &[f1, f2] : files_map1)
            d->addFile(rd, f1, f2);

        // unique deps
        SW_UNIMPLEMENTED;
        /*for (auto &dep : t.getDependencies())
        {
            // filter out predefined targets
            if (b.isPredefinedTarget(dep->getUnresolvedPackage()))
                continue;
            d->dependencies.insert(dep->getUnresolvedPackage());
        }*/

        m.emplace(pkg, std::move(d));
        if (iv)
            SW_UNIMPLEMENTED;
            //(*iv)[&tgts.getInput().getInput()].push_back(pkg);
    }
    return m;
}

static void input_check(const sw::Specification &spec)
{
    // if we have empty spec, we must provide source somehow
    // we have command line options for this:
    // TODO: implement necessary checks

    if (spec.files.getData().empty())
        throw SW_RUNTIME_ERROR("Specification must contain at least one file.");
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

    auto sources = fetch(*b);
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
    SW_UNIMPLEMENTED;
    //b->setTargetsToBuild();
    b->resolvePackages();
    //b->loadPackages();
    b->prepare();

    std::map<const sw::Input *, std::vector<sw::PackageId>> iv;
    auto m = getPackages(*b, sources, &iv);

    // dbg purposes
    for (auto &[id, d] : m)
    {
        write_file(b->getBuildDirectory() / "upload" / id.toString() += ".json", d->toJson().dump());
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
    auto current_remote = us.getRemotes(true).begin()->get();
    if (!getOptions().options_upload.upload_remote.empty())
        current_remote = &find_remote(us, getOptions().options_upload.upload_remote);

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
