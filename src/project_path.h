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

#pragma once

#include "common.h"

#define ROOT_PROJECT_PATH(name) \
    static ProjectPath name() \
    { \
        return ProjectPath(#name); \
    }

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

    String toString() const;
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
    ProjectPath operator/(const String &e)
    {
        if (e.empty())
            return *this;
        auto tmp = *this;
        tmp.path_elements.push_back(e);
        return tmp;
    }
    ProjectPath operator/(const ProjectPath &e)
    {
        auto tmp = *this;
        tmp.path_elements.insert(tmp.path_elements.end(), e.path_elements.begin(), e.path_elements.end());
        return tmp;
    }
    ProjectPath &operator/=(const String &e)
    {
        return *this = *this / e;
    }
    ProjectPath &operator/=(const ProjectPath &e)
    {
        return *this = *this / e;
    }

    ProjectPath operator[](PathElementType e) const
    {
        switch (e)
        {
        case PathElementType::Namespace:
            if (path_elements.empty())
                return *this;
            return PathElements{ path_elements.begin(), path_elements.begin() + 1 };
        case PathElementType::Owner:
            if (path_elements.size() < 2)
                return *this;
            return PathElements{ path_elements.begin() + 1, path_elements.begin() + 2 };
        case PathElementType::Tail:
            if (path_elements.size() >= 2)
                return *this;
            return PathElements{ path_elements.begin() + 2, path_elements.end() };
        }
        return *this;
    }

    bool has_namespace() const;

    bool is_absolute() const;
    bool is_relative() const;

    PathElement get_owner() const;

    bool operator<(const ProjectPath &rhs) const;

    ROOT_PROJECT_PATH(org);
    ROOT_PROJECT_PATH(com);
    ROOT_PROJECT_PATH(pvt);

private:
    PathElements path_elements;
};