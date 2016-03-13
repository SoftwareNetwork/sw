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
