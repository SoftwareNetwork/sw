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

#pragma once

#include "cppan_string.h"
#include "filesystem.h"
#include "yaml.h"

#include <primitives/hash.h>

#define ROOT_PROJECT_PATH(name)           \
    static ProjectPath name()             \
    {                                     \
        return ProjectPath(#name);        \
    }                                     \
    bool is_##name() const                \
    {                                     \
        if (path_elements.empty())        \
            return false;                 \
        return path_elements[0] == #name; \
    }

bool is_valid_project_path_symbol(int c);

enum class PathElementType
{
    Namespace,
    Owner,
    Tail,
};

class ProjectPath
{
public:
    using PathElement = String;
    using PathElements = std::vector<PathElement>;

    using iterator = PathElements::iterator;
    using const_iterator = PathElements::const_iterator;

public:
    ProjectPath() {}
    ProjectPath(const PathElements &pe);
    ProjectPath(String s);

    String toString(const String &delim = ".") const;
    String toPath() const;
    path toFileSystemPath() const;

    iterator begin()
    {
        return path_elements.begin();
    }
    iterator end()
    {
        return path_elements.end();
    }

    const_iterator begin() const
    {
        return path_elements.begin();
    }
    const_iterator end() const
    {
        return path_elements.end();
    }

    size_t size() const
    {
        return path_elements.size();
    }

    bool empty() const
    {
        return path_elements.empty();
    }

    auto back() const
    {
        return path_elements.back();
    }
    ProjectPath back(const ProjectPath &root) const;

    void push_back(const PathElement &pe);

    bool operator==(const ProjectPath &rhs) const
    {
        return path_elements == rhs.path_elements;
    }
    bool operator!=(const ProjectPath &rhs) const
    {
        return !operator==(rhs);
    }

    ProjectPath &operator=(const String &s)
    {
        ProjectPath tmp(s);
        std::swap(tmp, *this);
        return *this;
    }

    ProjectPath operator/(const String &e) const;
    ProjectPath operator/(const ProjectPath &e) const;
    ProjectPath &operator/=(const String &e);
    ProjectPath &operator/=(const ProjectPath &e);

    ProjectPath operator[](PathElementType e) const;

    bool has_namespace() const;

    bool is_absolute(const String &username = String()) const;
    bool is_relative(const String &username = String()) const;
    bool is_root_of(const ProjectPath &rhs) const;

    PathElement get_owner() const;
    auto get_name() const { return back(); }
    ProjectPath parent() const { return PathElements(path_elements.begin(), path_elements.end() - 1); }

    ProjectPath slice(int start, int end = -1) const;

    bool operator<(const ProjectPath &rhs) const;

    operator String() const
    {
        return toString();
    }

    ROOT_PROJECT_PATH(com);
    ROOT_PROJECT_PATH(loc);
    ROOT_PROJECT_PATH(org);
    ROOT_PROJECT_PATH(pvt);

private:
    PathElements path_elements;

    friend struct std::hash<ProjectPath>;
};

void fix_root_project(yaml &root, const ProjectPath &ppath);

namespace std
{

template<> struct hash<ProjectPath>
{
    size_t operator()(const ProjectPath& ppath) const
    {
        size_t h = 0;
        for (const auto &e : ppath.path_elements)
            hash_combine(h, std::hash<String>()(e));
        return h;
    }
};

}
