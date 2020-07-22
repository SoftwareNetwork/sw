// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2020 Egor Pugin <egor.pugin@gmail.com>

#include "../commands.h"

#include <sw/support/filesystem.h>

#include <primitives/pack.h>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "pack");

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
            for (auto ty : types)
            {
                auto files = t->getFiles(ty);
                std::map<path, path> files2;
                for (auto &[k, v] : files)
                {
                    if ((ty == sw::StorageFileType::SourceArchive && v.isGenerated()) || v.isFromOtherTarget())
                        continue;
                    files2[k] = v.getPath();
                }
                if (files2.empty())
                {
                    LOG_INFO(logger, "No files for " << pkg.toString() << ": " << toString(ty));
                    continue;
                }
                LOG_INFO(logger, "Packing " << pkg.toString() << ": " << toString(ty));
                pack_files(std::to_string((int)ty) + "-" + sw::support::make_archive_name(pkg.toString()), files2);
            }
        }
    }
}
