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
#include "package.h"

#include <set>

struct Remote;

struct DownloadDependency : public Package
{
    using DownloadDependencies = std::map<ProjectVersionId, DownloadDependency>;
    using DbDependencies = std::map<String, DownloadDependency>;

    // extended data
    ProjectVersionId id = 0;
    String sha256;

    // own data (private)
    const Remote *remote = nullptr;
    std::set<ProjectVersionId> id_dependencies;
    DbDependencies dependencies;
    DownloadDependencies *map_ptr = nullptr;

public:
    void setDependencyIds(const std::set<ProjectVersionId> &ids) { id_dependencies = ids; }

    Packages getDirectDependencies() const;
    Packages getIndirectDependencies(const Packages &known_deps = Packages()) const;
    DownloadDependencies getDependencies() const;

private:
    void getIndirectDependencies(std::set<ProjectVersionId> &deps) const;
};

using DownloadDependencies = DownloadDependency::DownloadDependencies;
