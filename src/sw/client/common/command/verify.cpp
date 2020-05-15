/*
 * SW - Build System and Package Manager
 * Copyright (C) 2020 Egor Pugin
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

#include <primitives/pack.h>
#include <primitives/yaml.h>
#include <nlohmann/json.hpp>
#include <sw/core/specification.h>
#include <sw/support/source.h>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "verify");

SUBCOMMAND_DECL(verify)
{
    // get package infos
    sw::UnresolvedPackage u = getOptions().options_verify.verify_arg;
    auto ml = getContext().install(sw::UnresolvedPackages{ u });
    auto &lp = ml.find(u)->second;
    auto m = getContext().resolve(sw::UnresolvedPackages{ u }, getContext().getRemoteStorages());
    auto &p = m[u];
    auto &src = p->getData().source;
    if (src.empty())
        throw SW_RUNTIME_ERROR("Empty source");

    // download source
    auto js = nlohmann::json::parse(src);
    auto s = sw::Source::load(js);
    auto dir = fs::current_path() / SW_BINARY_DIR / "verify" / unique_path();
    SCOPE_EXIT{ fs::remove_all(dir); };
    LOG_INFO(logger, "Downloading remote source:");
    LOG_INFO(logger, js.dump(4));
    s->download(dir);
    dir /= sw::findRootDirectory(dir); // pass found regex or files for better root dir lookup

    // setup build to get package files
    auto b = getContext().createBuild();
    auto inputs = b->addInput(p->toString());
    SW_CHECK(inputs.size() == 1);

    auto ts = createInitialSettings();
    ts["driver"]["source-dir-for-source"][s->getHash()] = normalize_path(dir);
    ts["driver"]["force-source"] = src;
    sw::InputWithSettings i(inputs[0]);
    i.addSettings(ts);
    b->addInput(i);
    b->loadInputs();

    SW_CHECK(!b->getTargets()[lp].empty());
    auto &t = **b->getTargets()[lp].begin();

    // get files and normalize them
    FilesSorted files;
    for (auto &f : t.getSourceFiles())
        files.insert(f.lexically_normal());

    // we put files under SW_SDIR_NAME to keep space near it
    // e.g. for patch dir or other dirs (server provided files)
    // we might unpack to other dir, but server could push service files in neighbor dirs like gpg keys etc
    auto files_map = primitives::pack::prepare_files(files, dir.lexically_normal());

    // move everything to sdir
    for (auto &[k, v] : files_map)
        v = sw::getSourceDirectoryName() / v;

    // add specs
    auto real_inputs = getContext().detectInputs(lp.getDirSrc2());
    SW_CHECK(real_inputs.size() == 1); // for now
    for (auto &[rel, _] : real_inputs[0]->getSpecification().files.getData())
        files_map[dir / rel] = sw::getSourceDirectoryName() / rel;

    // pack
    String pack_err;
    auto archive_name = dir / "sw.tar.gz";
    if (!pack_files(archive_name, files_map))
        throw SW_RUNTIME_ERROR(p->toString() + ": archive write failed: " + pack_err);

    // get hash
    auto hash = strong_file_hash_blake2b_sha3(archive_name);
    if (hash != p->getData().getHash(sw::StorageFileType::SourceArchive))
        throw SW_RUNTIME_ERROR("Archives do not match!");

    // success!
    LOG_INFO(logger, "Archives are the same.");
    LOG_INFO(logger, "Verified OK!");
}
