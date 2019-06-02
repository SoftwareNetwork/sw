// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "program_storage.h"

#include "target/base.h"
#include "source_file.h"
#include "build.h"
#include "target/native.h"

//#include <primitives/log.h>
//DECLARE_STATIC_LOGGER(logger, "program_storage");

namespace sw
{

ProgramStorage::~ProgramStorage() = default;

/*void ProgramStorage::setExtensionProgram(const String &ext, const PackageId &pkg)
{
    extensions.insert_or_assign(ext, pkg);
}*/

void ProgramStorage::setExtensionProgram(const String &ext, const ProgramPtr &p)
{
    extensions.insert_or_assign(ext, p);
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
    //auto pkg = t->getSolution().swctx.resolve(p);
    extensions.insert_or_assign(ext, p);

    // add a dependency to current target
    if (auto t = dynamic_cast<NativeCompiledTarget*>(this); t)
        (*t + std::make_shared<Dependency>(p))->setDummy(true);
}

Program *ProgramStorage::getProgram(const String &ext) const
{
    auto i = extensions.find(ext);
    if (i == extensions.end())
        return {};
    auto p = std::get_if<ProgramPtr>(&i->second);
    if (!p)
        return {};
    return p->get();
}

std::optional<UnresolvedPackage> ProgramStorage::getExtPackage(const String &ext) const
{
    auto i = extensions.find(ext);
    if (i == extensions.end())
        return {};
    auto p = std::get_if<UnresolvedPackage>(&i->second);
    if (!p)
        return {};
    return *p;
}

bool ProgramStorage::hasExtension(const String &ext) const
{
    return extensions.find(ext) != extensions.end();
}

void ProgramStorage::clearExtensions()
{
    extensions.clear();
}

void ProgramStorage::removeExtension(const String &ext)
{
    extensions.erase(ext);
}

}
