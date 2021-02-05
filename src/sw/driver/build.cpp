// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#include "build.h"

#include "entry_point.h"
#include "frontend/cppan/yaml.h"
#include "functions.h"
#include "input.h"
#include "module.h"
#include "suffix.h"
#include "target/native.h"

#include <sw/builder/file_storage.h>
#include <sw/core/build.h>
#include <sw/core/sw_context.h>
#include <sw/manager/database.h>
#include <sw/manager/settings.h>
#include <sw/manager/storage.h>

#include <boost/algorithm/string.hpp>
#include <boost/range/combine.hpp>
#include <boost/thread/lock_types.hpp>
#include <primitives/emitter.h>
#include <primitives/date_time.h>
#include <primitives/executor.h>
#include <primitives/pack.h>
#include <primitives/symbol.h>
#include <primitives/templates.h>
#include <primitives/sw/settings_program_name.h>

#include <boost/dll.hpp>
#include <nlohmann/json.hpp>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "build");

namespace sw
{

ModuleSwappableData::ModuleSwappableData() {}
ModuleSwappableData::~ModuleSwappableData() {}

void ModuleSwappableData::addTarget(ITargetPtr p)
{
    added_targets.push_back(std::move(p));
}

void ModuleSwappableData::markAsDummy(const ITarget &in_t)
{
    for (auto i = added_targets.begin(); i != added_targets.end(); i++)
    {
        if (i->get() != &in_t)
            continue;
        dummy_children.emplace_back(std::move(*i));
        added_targets.erase(i);
        break;
    }
}

ModuleSwappableData::AddedTargets &ModuleSwappableData::getTargets()
{
    return added_targets;
}

Build::Build(SwBuild &mb)
    : checker(std::make_unique<Checker>(mb))
{
    main_build_ = &mb;
}

//Build::~Build() {}

// can be used in configs to load subdir configs
// s.build->loadModule("client/sw.cpp").call<void(Solution &)>("build", s);
/*Module Build::loadModule(const path &p) const
{
    auto fn2 = p;
    if (!fn2.is_absolute())
        fn2 = SourceDir / fn2;
    // driver->build_cpp_spec(swctx, p);
    //return getContext().getModuleStorage().get(dll);
}*/

const PackageSettings &ModuleSwappableData::getSettings() const
{
    if (current_settings)
        return *current_settings;
    if (known_target)
        return known_target->getId().getSettings();
    return *current_settings;
}

bool Build::isKnownTarget(const PackageName &p) const
{
    return !module_data.known_target || module_data.known_target->getId().getName() == p;
}

std::optional<path> Build::getSourceDir(const Source &s, const PackageVersion &v) const
{
    auto s2 = s.clone();
    s2->apply([&v](auto &&s) { return v.format(s); });
    if (dd)
    {
        auto i = dd->source_dirs_by_source.find(s2->getHash());
        if (i != dd->source_dirs_by_source.end())
            return i->second.getRequestedDirectory();
    }
    return {};
}

Resolver &Build::getResolver() const
{
    if (!module_data.resolver)
        throw SW_RUNTIME_ERROR("no resolver set");
    return *module_data.resolver;
}

const PackageSettings &Build::getExternalVariables() const
{
    return getMainBuild().getExternalVariables();
}

}
