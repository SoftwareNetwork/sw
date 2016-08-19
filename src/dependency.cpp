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

#include "dependency.h"

Dependency extractFromString(const String &target)
{
    ProjectPath p = target.substr(0, target.find('-'));
    Version v = target.substr(target.find('-') + 1);
    return{ p,v };
}

Dependencies DownloadDependency::getDirectDependencies() const
{
    Dependencies deps;
    for (auto d : dependencies)
    {
        auto &dep = (*map_ptr)[d];
        deps[dep.package.toString()] = dep;
    }
    deps.erase(package.toString()); // erase self
    return deps;
}

Dependencies DownloadDependency::getIndirectDependencies(const Dependencies &known_deps) const
{
    Dependencies deps = known_deps;
    for (auto d : dependencies)
    {
        auto &dep = (*map_ptr)[d];
        auto p = deps.insert({ dep.package.toString(), dep });
        if (p.second)
        {
            auto id = dep.getIndirectDependencies(deps);
            deps.insert(id.begin(), id.end());
        }
    }
    if (known_deps.empty())
    {
        // first call in a chain

        // erase direct deps
        for (auto &d : getDirectDependencies())
            deps.erase(d.first);

        // erase self
        deps.erase(package.toString());
    }
    return deps;
}

void DownloadDependency::getIndirectDependencies(std::set<int> &deps) const
{
    for (auto d : dependencies)
    {
        if (deps.find(d) != deps.end())
            continue;
        auto &dep = (*map_ptr)[d];
        deps.insert(d);
        dep.getIndirectDependencies(deps);
    }
}

DownloadDependencies DownloadDependency::getDependencies() const
{
    DownloadDependencies download_deps;

    // direct
    for (auto d : dependencies)
    {
        auto dep = (*map_ptr)[d];
        dep.flags.set(pfDirectDependency);
        download_deps[d] = dep;
    }

    // indirect
    std::set<int> indirect_deps;
    for (auto d : dependencies)
    {
        auto &dep = (*map_ptr)[d];
        dep.getIndirectDependencies(indirect_deps);
    }
    for (auto d : indirect_deps)
    {
        auto dep = (*map_ptr)[d];
        dep.flags.set(pfDirectDependency, false);
        download_deps.erase(d);
        download_deps[d] = dep;
    }

    return download_deps;
}
