// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "spec.h"

#include "package.h"
#include "property_tree.h"

#include <sw/support/http.h>

#include <primitives/command.h>

#define SPEC_FILE_EXTENSION ".cppan"
//#define SPEC_FILES_LOCATION "https://github.com/cppan/specs"s
#define SPEC_FILES_LOCATION "https://raw.githubusercontent.com/cppan/specs/master/"s

namespace sw
{

/*Specification download_specification(const Package &pkg)
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
}*/

}
