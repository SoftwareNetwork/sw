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
