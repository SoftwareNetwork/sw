// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2020 Egor Pugin <egor.pugin@gmail.com>

#include "../commands.h"

#include <sw/manager/storage.h>

#include <primitives/pack.h>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "pack");

struct ChildPathExtractor
{
    String root;

    ChildPathExtractor(const path &in_root)
    {
        root = normalize_path(in_root);
    }

    bool isUnderRoot(const path &p) const
    {
        auto s = normalize_path(p);
        return s.find(root) == 0;
    }

    path getRelativePath(const path &p) const
    {
        auto s = normalize_path(p);
        if (s.find(root) == 0)
            return s.substr(root.size() + 1); // plus slash
        throw SW_RUNTIME_ERROR("Not a relative path: " + s + ", root = " + root);
    }
};

SUBCOMMAND_DECL(pack)
{
    std::vector<sw::StorageFileType> types;
    for (auto i : getOptions().options_pack.typei)
        types.push_back((sw::StorageFileType)i);
    for (auto &i : getOptions().options_pack.type)
    {
        if (i == "source")
            types.push_back(sw::StorageFileType::SourceArchive);
        else if (i == "binary")
            types.push_back(sw::StorageFileType::RuntimeArchive);
        else
            SW_UNIMPLEMENTED;
    }
    if (types.empty())
        types.push_back(sw::StorageFileType::SourceArchive);

    auto b = createBuildAndPrepare({getInputs(), getOptions().input_settings_pairs});
    b->build();

    for (auto &[pkg,tgts] : b->getTargetsToBuild())
    {
        for (auto &t : tgts)
        {
            auto &is = t->getInterfaceSettings();
            auto sdir = is["source_dir"].getPathValue(getContext().getLocalStorage().storage_dir);
            auto bdir = is["binary_dir"].getPathValue(getContext().getLocalStorage().storage_dir).parent_path();

            ChildPathExtractor spe(sdir);
            ChildPathExtractor bpe(bdir);

            for (auto ty : types)
            {
                if (ty != sw::StorageFileType::SourceArchive && t->getPackage().getPath().isRelative())
                    throw SW_RUNTIME_ERROR("Only Source Archives are available for local packages");

                auto files = t->getFiles(ty);
                std::map<path, path> files2;
                for (auto &[k, v] : files)
                {
                    if ((ty == sw::StorageFileType::SourceArchive && v.isGenerated()) || v.isFromOtherTarget())
                        continue;
                    auto p = v.getPath();
                    if (ty == sw::StorageFileType::SourceArchive)
                        p = spe.getRelativePath(p);
                    else
                        p = bpe.getRelativePath(p);
                    if (p.empty())
                        throw SW_RUNTIME_ERROR("Cannot calc relative path");
                    files2[k] = p;
                }
                if (files2.empty())
                {
                    LOG_INFO(logger, "No files for " << pkg.toString() << ": " << toString(ty));
                    continue;
                }
                LOG_INFO(logger, "Packing " << pkg.toString() << ": " << toString(ty));
                for (auto &[k, v] : files2)
                    LOG_TRACE(logger, k << ": " << v);
                pack_files(std::to_string((int)ty) + "-" + sw::support::make_archive_name(pkg.toString()), files2);
            }
        }
    }
}
