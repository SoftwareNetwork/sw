// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "options.h"
#include "program.h"

#include <sw/manager/package_version_map.h>

#include <primitives/filesystem.h>

#include <optional>

namespace sw
{

struct FileStorage;
struct TargetBase;

struct SW_DRIVER_CPP_API ProgramStorage
{
    using ProgramType = ProgramPtr;
    using StorageMap = PackageVersionMapBase<ProgramType, std::unordered_map, primitives::version::VersionMap>;

    // make type polymorphic for dyncasts
    virtual ~ProgramStorage();

    // direct registration of program without extensions activation
    void registerProgram(const PackagePath &pp, const ProgramType &);
    void registerProgram(const TargetBase &t, const ProgramType &);
    void registerProgram(const PackageId &pp, const ProgramType &); // main

    // late resolving registration with potential activation
    void setExtensionProgram(const String &ext, const UnresolvedPackage &p);
    void setExtensionProgram(const String &ext, const ProgramType &p);
    void setExtensionProgram(const String &ext, const DependencyPtr &p);

    // activate by ppath/pkgid
    ProgramType activateProgram(const PackagePath &pp); // latest ver
    ProgramType activateProgram(const PackageId &pkg, bool exact_version = true);

    ProgramType getProgram(const PackagePath &pp) const; // latest ver
    ProgramType getProgram(const PackageId &pkg, bool exact_version = true) const;

    std::optional<PackageId> getPackage(const String &ext) const;

    void setFs(FileStorage *);
    void removeAllExtensions();
    void removeExtension(const String &ext);

    ProgramType::element_type *findProgramByExtension(const String &ext) const;
    bool hasExtension(const String &ext) const;

private:
    std::map<String, PackageId> extension_packages;
    std::map<String, ProgramType> extension_programs;
    StorageMap registered_programs;

    void activateProgram1(const ProgramType &);

    std::optional<PackageId> getPackage(const PackagePath &pp) const; // latest ver
    std::optional<PackageId> getPackage(const PackageId &pkg, bool exact_version = true) const;
};

}
