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

String toString(ProjectType e)
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
    default:
        break;
    }
    return std::to_string(toIndex(e));
}

String toString(SettingsType e)
{
    switch (e)
    {
    case SettingsType::Local:
        return "local";
    case SettingsType::User:
        return "user";
    case SettingsType::System:
        return "system";
    default:
        break;
    }
    return std::to_string(toIndex(e));
}

String getFlagsString(const ProjectFlags &flags)
{
    // keep sorted
    String str;
#define ADD_FLAG(f, s) if (flags[f]) str += s
    ADD_FLAG(pfExecutable, "E");
    ADD_FLAG(pfHeaderOnly, "H");
    ADD_FLAG(pfIncludeDirectoriesOnly, "I");
    ADD_FLAG(pfPrivateDependency, "P");
    return str;
}
