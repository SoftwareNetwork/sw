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
    get_val(s.hash, "hash");
    get_val(created, "created");

    s.package.version = Version(version);
    s.source = load_source(p);
    s.created = string2timepoint(created);

    return s;
}
