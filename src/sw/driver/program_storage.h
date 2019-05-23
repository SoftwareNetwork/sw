// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "options.h"

#include <sw/builder/program.h>

#include <optional>
#include <variant>

namespace sw
{

struct Target;

struct SW_DRIVER_CPP_API ProgramStorage
{
    // make type polymorphic for dyncasts
    virtual ~ProgramStorage();

    // late resolving registration with potential activation
    //void setExtensionProgram(const String &ext, const PackageId &);
    void setExtensionProgram(const String &ext, const UnresolvedPackage &);
    void setExtensionProgram(const String &ext, const DependencyPtr &);
    void setExtensionProgram(const String &ext, const ProgramPtr &);

    bool hasExtension(const String &ext) const;
    std::optional<UnresolvedPackage> getExtPackage(const String &ext) const;
    Program *getProgram(const String &ext) const;

    void clearExtensions();
    void removeExtension(const String &ext);

private:
    std::map<String, std::variant<UnresolvedPackage, ProgramPtr>> extensions;
};

}
