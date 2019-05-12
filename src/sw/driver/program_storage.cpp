// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "program_storage.h"

#include "target/base.h"
#include "source_file.h"
#include "build.h"
#include "sw_context.h"
#include "target/native.h"

//#include <primitives/log.h>
//DECLARE_STATIC_LOGGER(logger, "program_storage");

namespace sw
{

ProgramStorage::~ProgramStorage() = default;

void ProgramStorage::setExtensionProgram(const String &ext, const PackageId &pkg)
{
    extension_packages.insert_or_assign(ext, pkg);
}

void ProgramStorage::setExtensionProgram(const String &ext, const ProgramPtr &p)
{
    extension_programs.insert_or_assign(ext, p);
}

void ProgramStorage::setExtensionProgram(const String &ext, const DependencyPtr &d)
{
    setExtensionProgram(ext, d->getPackage());
}

void ProgramStorage::setExtensionProgram(const String &ext, const UnresolvedPackage &p)
{
    auto t = dynamic_cast<TargetBase*>(this);
    if (!t)
        throw SW_RUNTIME_ERROR("not a target");

    // late resolve version
    auto pkg = t->getSolution().swctx.resolve(p);
    extension_packages.insert_or_assign(ext, pkg);

    // add a dependency to current target
    if (auto t = dynamic_cast<NativeCompiledTarget*>(this); t)
        (*t + std::make_shared<Dependency>(p))->setDummy(true);
}

Program *ProgramStorage::getProgram(const String &ext) const
{
    auto i = extension_programs.find(ext);
    if (i == extension_programs.end())
        return {};
    return i->second.get();
}

std::optional<PackageId> ProgramStorage::getExtPackage(const String &ext) const
{
    auto i = extension_packages.find(ext);
    if (i == extension_packages.end())
        return {};
    return i->second;
}

int ProgramStorage::hasExtension(const String &ext) const
{
    if (extension_programs.find(ext) != extension_programs.end())
        return HAS_PROGRAM_EXTENSION;
    if (extension_packages.find(ext) != extension_packages.end())
        return HAS_PACKAGE_EXTENSION;
    return NO_EXTENSION;
}

void ProgramStorage::clearExtensions()
{
    extension_packages.clear();
    extension_programs.clear();
}

void ProgramStorage::removeExtension(const String &ext)
{
    extension_packages.erase(ext);
    extension_programs.erase(ext);
}

}
