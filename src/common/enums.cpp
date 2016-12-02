/*
 * Copyright (c) 2016, Egor Pugin
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     1. Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *     2. Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the distribution.
 *     3. Neither the name of the copyright holder nor the names of
 *        its contributors may be used to endorse or promote products
 *        derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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

std::string toString(ConfigType e)
{
    switch (e)
    {
    case ConfigType::Local:
        return "local";
    case ConfigType::User:
        return "user";
    case ConfigType::System:
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
