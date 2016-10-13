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

#include "version.h"

const std::regex r_branch_name(R"(([a-zA-Z_][a-zA-Z0-9_-]*))");
const std::regex r_version1(R"((\d+))");
const std::regex r_version2(R"((\d+).(\d+))");
const std::regex r_version3(R"((-?\d+).(-?\d+).(-?\d+))");

Version::Version(ProjectVersionNumber ma, ProjectVersionNumber mi, ProjectVersionNumber pa)
    : major(ma), minor(mi), patch(pa)
{

}

Version::Version(const String &s)
{
    if (s == "*")
        return;
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
    }
    else
        throw std::runtime_error("Bad version");
    if (!isValid())
        throw std::runtime_error("Bad version");
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
    if (major == -1 && minor == -1 && patch == -1)
        return "*";
    String s;
    s += std::to_string(major) + ".";
    if (minor != -1)
        s += std::to_string(minor) + ".";
    if (patch != -1)
        s += std::to_string(patch) + ".";
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
    if (major > 9999 || minor > 9999 || patch > 9999) // increase or remove limits later
        return false;
    return true;
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
