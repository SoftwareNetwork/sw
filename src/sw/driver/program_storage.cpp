// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "program_storage.h"

#include "target/base.h"
#include "source_file.h"
#include "solution.h"
#include "target/native.h"

#include <sw/builder/sw_context.h>

#include <primitives/hash.h>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "language");

namespace sw
{

ProgramStorage::~ProgramStorage() = default;

void ProgramStorage::registerProgram(const PackagePath &pp, const ProgramType &p)
{
    registerProgram({ pp,p->getVersion() }, p);
}

void ProgramStorage::registerProgram(const TargetBase &t, const ProgramType &p)
{
    registerProgram(t.getPackage(), p);
}

void ProgramStorage::registerProgram(const PackageId &pp, const ProgramType &in_p)
{
    String exts;
    if (auto p = std::dynamic_pointer_cast<FileToFileTransformProgram>(in_p))
    {
        if (!p->input_extensions.empty())
        {
            for (auto &e : p->input_extensions)
                exts += e + ", ";
            exts.resize(exts.size() - 2);
            exts = ", extensions: " + exts;
        }
    }
    LOG_DEBUG(logger, "registering program: " + pp.toString() + ", path: " + in_p->file.u8string() + exts);

    auto &p2 = registered_programs[pp.ppath][pp.version] = in_p;
    if (auto t = dynamic_cast<TargetBase*>(this); t)
        p2->fs = t->getSolution()->fs;
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
    auto pkg = t->getSolution()->swctx.resolve(p);
    extension_packages.insert_or_assign(ext, pkg);

    // add a dependency to current target
    if (auto t = dynamic_cast<NativeExecutedTarget*>(this); t)
        (*t + std::make_shared<Dependency>(p))->setDummy(true);
}

void ProgramStorage::setExtensionProgram(const String &ext, const ProgramType &p)
{
    extension_programs.insert_or_assign(ext, p);
}

void ProgramStorage::activateProgram1(const ProgramType &in_p)
{
    if (!in_p)
        return;
    if (auto p = std::dynamic_pointer_cast<FileToFileTransformProgram>(in_p))
    {
        for (auto &e : p->input_extensions)
            extension_programs.insert_or_assign(e, p);
    }
}

ProgramStorage::ProgramType ProgramStorage::activateProgram(const PackagePath &pp)
{
    auto p = getProgram(pp);
    activateProgram1(p);
    return p;
}

ProgramStorage::ProgramType ProgramStorage::activateProgram(const PackageId &pkg, bool exact_version)
{
    auto p = getProgram(pkg, exact_version);
    activateProgram1(p);
    return p;
}

template <class T>
static std::optional<Version> select_version(T &v)
{
    if (v.empty())
        return {};
    if (!v.empty_releases())
        return v.rbegin_releases()->first;
    return v.rbegin()->first;
}

ProgramStorage::ProgramType ProgramStorage::getProgram(const PackagePath &pp) const
{
    auto p = getPackage(pp);
    if (!p)
        return {};
    return registered_programs.find(*p)->second;
}

ProgramStorage::ProgramType ProgramStorage::getProgram(const PackageId &pkg, bool exact_version) const
{
    auto p = getPackage(pkg, exact_version);
    if (!p)
        return {};
    return registered_programs.find(*p)->second;
}

std::optional<PackageId> ProgramStorage::getPackage(const PackagePath &pp) const
{
    auto i = registered_programs.find(pp);
    if (i == registered_programs.end(pp))
        return {};
    if (auto v = select_version(i->second); v)
        return getPackage({ pp, *v }, true);
    return {};
}

std::optional<PackageId> ProgramStorage::getPackage(const PackageId &pkg, bool exact_version) const
{
    auto vi = registered_programs.find(pkg.ppath);
    if (vi == registered_programs.end(pkg.ppath))
        return {};
    auto &v = vi->second;
    auto L = v.find(pkg.version);
    if (L == v.end())
    {
        if (exact_version)
            return {};
        auto i = primitives::version::findBestMatch(v.rbegin(), v.rend(), pkg.version, true);
        if (i == v.rend())
            return {};
        L = i.base();
        L--;
    }
    return PackageId{ pkg.ppath, L->first };
}

ProgramStorage::ProgramType::element_type *ProgramStorage::findProgramByExtension(const String &ext) const
{
    auto i = extension_programs.find(ext);
    if (i == extension_programs.end())
    {
        auto i = extension_packages.find(ext);
        if (i == extension_packages.end())
            return {};
        auto p = getProgram(i->second);
        if (!p)
            return {};
        return p.get();
    }
    if (!i->second)
        return {};
    return i->second.get();
}

std::optional<PackageId> ProgramStorage::getPackage(const String &ext) const
{
    auto i = extension_packages.find(ext);
    if (i == extension_packages.end())
        return {};
    return i->second;
}

void ProgramStorage::setFs(FileStorage *fs)
{
    for (auto &[_, p] : registered_programs)
        p->fs = fs;
}

bool ProgramStorage::hasExtension(const String &ext) const
{
    return
        extension_packages.find(ext) != extension_packages.end() ||
        extension_programs.find(ext) != extension_programs.end();
}

void ProgramStorage::removeAllExtensions()
{
    extension_packages.clear();
    extension_programs.clear();
    registered_programs.clear();
}

void ProgramStorage::removeExtension(const String &ext)
{
    extension_packages.erase(ext);
    extension_programs.erase(ext);
}

}
