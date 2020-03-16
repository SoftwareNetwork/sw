/*
 * SW - Build System and Package Manager
 * Copyright (C) 2017-2020 Egor Pugin
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

#include "program_storage.h"

#include "target/base.h"
#include "source_file.h"
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

    // also add (yes, duplicate!) passed dptr
    // add a dependency to current target
    if (auto t = dynamic_cast<NativeCompiledTarget *>(this); t)
        t->addDummyDependency(d);
}

void ProgramStorage::setExtensionProgram(const String &ext, const UnresolvedPackage &p)
{
    auto t = dynamic_cast<Target*>(this);
    if (!t)
        throw SW_RUNTIME_ERROR("not a target");

    // late resolve version
    //auto pkg = t->getSolution().swctx.resolve(p);
    extensions.insert_or_assign(ext, p);

    // add a dependency to current target
    if (auto t = dynamic_cast<NativeCompiledTarget *>(this); t)
        t->addDummyDependency(std::make_shared<Dependency>(p));
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
