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

#include "version.h"

#include <regex>

const std::regex r_branch_name(R"(([a-zA-Z_][a-zA-Z0-9_-]*))");
const std::regex r_version1(R"((\d+))");
const std::regex r_version2(R"((\d+)\.(\d+))");
const std::regex r_version3(R"((\d+)\.(\d+)\.(\d+))");

Version::Version(ProjectVersionNumber ma, ProjectVersionNumber mi, ProjectVersionNumber pa)
    : major(ma), minor(mi), patch(pa)
{
}

Version::Version(const String &s)
{
    if (s == "*")
    {
        type = VersionType::Any;
        return;
    }

    if (s == "=")
    {
        type = VersionType::Equal;
        return;
    }

    type = VersionType::Version;

    std::smatch m;
    if (std::regex_match(s, m, r_version3))
    {
        major = std::stoi(m[1].str());
        minor = std::stoi(m[2].str());
        patch = std::stoi(m[3].str());
    }
    else if (std::regex_match(s, m, r_version2))
    {
        major = std::stoi(m[1].str());
        minor = std::stoi(m[2].str());
    }
    else if (std::regex_match(s, m, r_version1))
    {
        major = std::stoi(m[1].str());
    }
    else if (std::regex_match(s, m, r_branch_name))
    {
        branch = m[1].str();
        String error;
        if (!check_branch_name(branch, &error))
            throw std::runtime_error(error);
        type = VersionType::Branch;
    }
    else
    {
        type = VersionType::Any;
        throw std::runtime_error("Bad version");
    }

    if (!isValid())
    {
        type = VersionType::Any;
        throw std::runtime_error("Bad version");
    }
}

String Version::toString() const
{
    if (!branch.empty())
        return branch;
    String s;
    s += std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
    return s;
}

String Version::toAnyVersion() const
{
    if (!branch.empty())
        return branch;
    if (type == VersionType::Equal)
        return "=";
    if (major == -1 && minor == -1 && patch == -1)
        return "*";
    String s;
    s += std::to_string(major) + ".";
    if (minor != -1)
    {
        s += std::to_string(minor) + ".";
    }
    if (patch != -1)
    {
        s += std::to_string(patch) + ".";
    }
    s.resize(s.size() - 1);
    return s;
}

path Version::toPath() const
{
    if (!branch.empty())
        return branch;
    path p;
    p /= std::to_string(major);
    p /= std::to_string(minor);
    p /= std::to_string(patch);
    return p;
}

bool Version::isValid() const
{
    if (!branch.empty())
        return check_branch_name(branch);
    if (major == 0 && minor == 0 && patch == 0)
        return false;
    if (major < -1 || minor < -1 || patch < -1)
        return false;
    return true;
}

bool Version::isBranch() const
{
    return !branch.empty();
}

bool Version::isVersion() const
{
    return !isBranch();
}

bool Version::operator<(const Version &rhs) const
{
    if (isBranch() && rhs.isBranch())
        return branch < rhs.branch;
    if (isBranch())
        return true;
    if (rhs.isBranch())
        return false;
    return std::tie(major, minor, patch) < std::tie(rhs.major, rhs.minor, rhs.patch);
}

bool Version::operator==(const Version &rhs) const
{
    if (isBranch() && rhs.isBranch())
        return branch == rhs.branch;
    if (isBranch() || rhs.isBranch())
        return false;
    return std::tie(major, minor, patch) == std::tie(rhs.major, rhs.minor, rhs.patch);
}

bool Version::operator!=(const Version &rhs) const
{
    return !operator==(rhs);
}

bool Version::canBe(const Version &rhs) const
{
    if (*this == rhs)
        return true;

    // *.*.* canBe anything
    if (major == -1 && minor == -1 && patch == -1)
        return true;

    // 1.*.* == 1.*.*
    if (major == rhs.major && minor == -1 && patch == -1)
        return true;

    // 1.2.* == 1.2.*
    if (major == rhs.major && minor == rhs.minor && patch == -1)
        return true;

    return false;
}

bool Version::check_branch_name(const String &n, String *error)
{
    if (!std::regex_match(n, r_branch_name))
    {
        if (error)
            *error = "Branch name should be a-zA-Z0-9_- starting with letter or _";
        return false;
    }
    return true;
}
