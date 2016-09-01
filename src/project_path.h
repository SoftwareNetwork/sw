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
    ProjectPath operator/(const String &e) const
    {
        if (e.empty())
            return *this;
        auto tmp = *this;
        tmp.path_elements.push_back(e);
        return tmp;
    }
    ProjectPath operator/(const ProjectPath &e) const
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