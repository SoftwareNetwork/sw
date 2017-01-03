/*
 * Copyright (C) 2016-2017, Egor Pugin
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
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

std::string toString(SettingsType e)
{
    switch (e)
    {
    case SettingsType::Local:
        return "local";
    case SettingsType::User:
        return "user";
    case SettingsType::System:
        return "system";
    }
    return std::to_string(toIndex(e));
}

std::string getFlagsString(const ProjectFlags &flags)
{
    std::string s;
    if (flags[pfHeaderOnly])
        s += "H";
    if (flags[pfExecutable])
        s += "E";
    return s;
}
