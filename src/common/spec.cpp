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

#include "spec.h"

#include "command.h"
#include "http.h"
#include "package.h"
#include "property_tree.h"

//#define SPEC_FILES_LOCATION "https://github.com/cppan/specs"s
#define SPEC_FILES_LOCATION "https://raw.githubusercontent.com/cppan/specs/master/"s

Specification download_specification(const Package &pkg)
{
    auto url = path(SPEC_FILES_LOCATION) / pkg.ppath.toFileSystemPath() / (pkg.version.toString() + SPEC_FILE_EXTENSION);
    auto spec = download_file(normalize_path(url));
    return read_specification(spec);
}

Specification read_specification(const String &s)
{
    auto p = string2ptree(s);
    return read_specification(p);
}

Specification read_specification(const ptree &p)
{
    Specification s;

    auto get_val = [&p](auto &v, auto &&key)
    {
        if (p.find(key) != p.not_found())
            v = p.get<std::remove_reference_t<String>>(key);
    };

    String version;
    String created;

    get_val(s.package.ppath, "project");
    get_val(version, "version");
    get_val(s.cppan, "cppan");
    get_val(s.sha256, "sha256");
    get_val(created, "created");

    s.package.version = Version(version);
    s.source = load_source(p);
    s.created = string2timepoint(created);

    return s;
}
