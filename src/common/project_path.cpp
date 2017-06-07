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

#include "project_path.h"

#include "enums.h"

bool is_valid_project_path_symbol(int c)
{
    return
        c > 0 && c <= 127 &&
        (isalnum(c) || c == '.' || c == '_')
        ;
}

void fix_root_project(yaml &root, const ProjectPath &ppath)
{
    auto rp = root["root_project"];
    if (!rp.IsDefined())
    {
        rp = ppath.toString();
        return;
    }
    if (!ppath.is_root_of(rp.as<String>()))
        rp = ppath.toString();
}

ProjectPath::ProjectPath(String s)
{
    if (s.size() > 2048)
        throw std::runtime_error("Too long project path (must be <= 2048)");

    auto prev = s.begin();
    for (auto i = s.begin(); i != s.end(); ++i)
    {
        auto &c = *i;
        if (!is_valid_project_path_symbol(c))
            throw std::runtime_error("Bad symbol in project name");
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

String ProjectPath::toString(const String &delim) const
{
    String p;
    if (path_elements.empty())
        return p;
    for (auto &e : path_elements)
        p += e + delim;
    p.resize(p.size() - delim.size());
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
    // TODO: replace with hash, affects both server and client
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
    // ??
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
        path_elements[0] == com().path_elements[0] ||
        path_elements[0] == loc().path_elements[0])
        return true;
    return false;
}

ProjectPath::PathElement ProjectPath::get_owner() const
{
    if (path_elements.size() < 2)
        return PathElement();
    return path_elements[1];
}

bool ProjectPath::is_absolute(const String &username) const
{
    if (!has_namespace())
        return false;
    if (username.empty())
    {
        if (path_elements.size() > 1)
            return true;
        return false;
    }
    if (path_elements.size() > 2 && path_elements[1] == username)
        return true;
    return false;
}

bool ProjectPath::is_relative(const String &username) const
{
    return !is_absolute(username);
}

ProjectPath ProjectPath::operator[](PathElementType e) const
{
    if (path_elements.empty())
        return *this;
    switch (e)
    {
    case PathElementType::Namespace:
        return path_elements[0];
    case PathElementType::Owner:
        return get_owner();
    case PathElementType::Tail:
        if (path_elements.size() < 2)
            return ProjectPath();
        return PathElements{ path_elements.begin() + 2, path_elements.end() };
    }
    return *this;
}

bool ProjectPath::is_root_of(const ProjectPath &rhs) const
{
    if (path_elements.size() >= rhs.path_elements.size())
        return false;
    for (size_t i = 0; i < path_elements.size(); i++)
    {
        if (path_elements[i] != rhs.path_elements[i])
            return false;
    }
    return true;
}

ProjectPath ProjectPath::back(const ProjectPath &root) const
{
    ProjectPath p;
    if (!root.is_root_of(*this))
        return p;
    for (size_t i = 0; i < root.path_elements.size(); i++)
    {
        if (path_elements[i] != root.path_elements[i])
        {
            p.path_elements.assign(path_elements.begin() + i, path_elements.end());
            break;
        }
    }
    if (p.path_elements.empty())
        p.path_elements.assign(path_elements.end() - (path_elements.size() - root.path_elements.size()), path_elements.end());
    return p;
}

void ProjectPath::push_back(const PathElement &pe)
{
    path_elements.push_back(pe);
}

ProjectPath ProjectPath::operator/(const String &e) const
{
    if (e.empty())
        return *this;
    auto tmp = *this;
    tmp.push_back(e);
    return tmp;
}

ProjectPath ProjectPath::operator/(const ProjectPath &e) const
{
    auto tmp = *this;
    tmp.path_elements.insert(tmp.path_elements.end(), e.path_elements.begin(), e.path_elements.end());
    return tmp;
}

ProjectPath &ProjectPath::operator/=(const String &e)
{
    return *this = *this / e;
}

ProjectPath &ProjectPath::operator/=(const ProjectPath &e)
{
    return *this = *this / e;
}

ProjectPath ProjectPath::slice(int start, int end) const
{
    auto p = *this;
    if (end == -1)
        p.path_elements = decltype(path_elements)(path_elements.begin() + start, path_elements.end());
    else
        p.path_elements = decltype(path_elements)(path_elements.begin() + start, path_elements.begin() + end);
    return p;
}
