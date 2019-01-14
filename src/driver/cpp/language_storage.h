// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <compiler.h>

#include <primitives/filesystem.h>

namespace sw
{

struct Language;
using LanguagePtr = std::shared_ptr<Language>;

struct TargetBase;

struct SW_DRIVER_CPP_API LanguageStorage
{
    //LanguageMap languages;
    std::map<String, PackageId> extensions;
    PackageVersionMapBase<LanguagePtr, std::unordered_map, std::map> user_defined_languages; // main languages!!! (UDL)
    PackageVersionMapBase<ProgramPtr, std::unordered_map, std::map> registered_programs; // main program storage

    virtual ~LanguageStorage();

    void registerProgramAndLanguage(const PackagePath &pp, const ProgramPtr &, const LanguagePtr &L);
    void registerProgramAndLanguage(const PackageId &t, const ProgramPtr &, const LanguagePtr &L);
    void registerProgramAndLanguage(const TargetBase &t, const ProgramPtr &, const LanguagePtr &L);

    void registerProgram(const PackagePath &pp, const ProgramPtr &);
    void registerProgram(const PackageId &pp, const ProgramPtr &);
    void registerProgram(const TargetBase &t, const ProgramPtr &);

    //void registerLanguage(const LanguagePtr &L); // allow unnamed UDLs?
    void registerLanguage(const PackageId &pkg, const LanguagePtr &L);
    void registerLanguage(const TargetBase &t, const LanguagePtr &L);

    void setExtensionLanguage(const String &ext, const UnresolvedPackage &p); // main
    void setExtensionLanguage(const String &ext, const LanguagePtr &p); // wrappers
    void setExtensionLanguage(const String &ext, const DependencyPtr &p); // wrappers

    bool activateLanguage(const PackagePath &pp); // latest ver
    bool activateLanguage(const PackageId &pkg);

    LanguagePtr getLanguage(const PackagePath &pp) const; // latest ver
    LanguagePtr getLanguage(const PackageId &pkg) const;

    ProgramPtr getProgram(const PackagePath &pp) const; // latest ver
    ProgramPtr getProgram(const PackageId &pkg) const;

    Program *findProgramByExtension(const String &ext) const;
    Language *findLanguageByExtension(const String &ext) const;
    std::optional<PackageId> findPackageIdByExtension(const String &ext) const;
};

}
