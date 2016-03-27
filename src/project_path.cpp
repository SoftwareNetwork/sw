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

#include "project_path.h"

ProjectPath::ProjectPath(String s)
{
    auto prev = s.begin();
    for (auto i = s.begin(); i != s.end(); ++i)
    {
        auto &c = *i;
        if (isupper(c))
            c = (char)tolower(c);
        if (c == '.')
        {
            path_elements.emplace_back(prev, i);
            prev = std::next(i);
        }
    }
    if (!s.empty())
        path_elements.emplace_back(prev, s.end());
}

ProjectPath::ProjectPath(const PathElements &pe)
    : path_elements(pe)
{
}

String ProjectPath::toString() const
{
    String p;
    if (path_elements.empty())
        return p;
    for (auto &e : path_elements)
        p += e + '.';
    p.resize(p.size() - 1);
    return p;
}

String ProjectPath::toPath() const
{
    String p = toString();
    std::replace(p.begin(), p.end(), '.', '/');
    return p;
}

path ProjectPath::toFileSystemPath() const
{
    path p;
    if (path_elements.empty())
        return p;
    int i = 0;
    for (auto &e : path_elements)
    {
        if (i++ == toIndex(PathElementType::Owner))
        {
            p /= e.substr(0, 1);
            p /= e.substr(0, 2);
        }
        p /= e;
    }
    return p;
}

bool ProjectPath::operator<(const ProjectPath &p) const
{
    if (path_elements.empty() && p.path_elements.empty())
        return false;
    if (path_elements.empty())
        return true;
    if (p.path_elements.empty())
        return false;
    auto &p0 = path_elements[0];
    auto &pp0 = p.path_elements[0];
    if (p0 == pp0)
        return path_elements < p.path_elements;
    if (p0 == "org")
        return true;
    if (pp0 == "org")
        return false;
    if (p0 == "pvt")
        return true;
    if (pp0 == "pvt")
        return false;
    return false;
}

bool ProjectPath::has_namespace() const
{
    if (path_elements.empty())
        return false;
    if (path_elements[0] == pvt().path_elements[0] ||
        path_elements[0] == org().path_elements[0] ||
        path_elements[0] == com().path_elements[0])
        return true;
    return false;
}

ProjectPath::PathElement ProjectPath::get_owner() const
{
    if (path_elements.size() < 2)
        return PathElement();
    return path_elements[1];
}

bool ProjectPath::is_absolute() const
{
    if (!has_namespace())
        return false;
    if (path_elements[0] == pvt().path_elements[0] && path_elements.size() > 2)
        return true;
    if (path_elements.size() > 1)
        return true;
    return false;
}

bool ProjectPath::is_relative() const
{
    return !is_absolute();
}
