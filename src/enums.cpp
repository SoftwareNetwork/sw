/*
 * C++ Archive Network Client
 * Copyright (C) 2016 Egor Pugin
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "enums.h"

std::string toString(ProjectType e)
{
    switch (e)
    {
    case ProjectType::Library:
        return "Library";
    case ProjectType::Executable:
        return "Executable";
    case ProjectType::RootProject:
        return "Root Project";
    case ProjectType::Directory:
        return "Directory";
    }
    return std::to_string(toIndex(e));
}

std::string toString(ProjectPathNamespace e)
{
#define CASE(name) \
    case ProjectPathNamespace::name: return #name

    switch (e)
    {
        CASE(com);
        CASE(org);
        CASE(pvt);
    }
    return std::string();
#undef CASE
}

std::string toString(PackagesDirType e)
{
    switch (e)
    {
    case PackagesDirType::Local:
        return "local";
    case PackagesDirType::User:
        return "user";
    case PackagesDirType::System:
        return "system";
    }
    return std::to_string(toIndex(e));
}
