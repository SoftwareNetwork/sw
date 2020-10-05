// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#include "program_storage.h"

#include "target/base.h"
#include "source_file.h"
#include "target/native.h"

//#include <primitives/log.h>
//DECLARE_STATIC_LOGGER(logger, "program_storage");

namespace sw
{

/*ProgramStorage::~ProgramStorage() = default;

void ProgramStorage::setExtensionProgram(const String &ext, const ProgramPtr &p)
{
    extensions.insert_or_assign(ext, p);
}

void ProgramStorage::setExtensionProgram(const String &ext, const DependencyPtr &d)
{
    extensions.insert_or_assign(ext, d);

    // also add (yes, duplicate!) passed dptr
    // add a dependency to current target
    if (auto t = dynamic_cast<Target *>(this); t)
        t->addDummyDependency(d);
}

void ProgramStorage::setExtensionProgram(const String &ext, const UnresolvedPackage &p)
{
    setExtensionProgram(ext, std::make_shared<Dependency>(p));
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

std::optional<DependencyPtr> ProgramStorage::getExtPackage(const String &ext) const
{
    auto i = extensions.find(ext);
    if (i == extensions.end())
        return {};
    auto p = std::get_if<DependencyPtr>(&i->second);
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
}*/

}
