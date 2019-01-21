// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "program.h"

#include "file_storage.h"

#include <sw/builder/command.h>

#include <boost/thread/shared_mutex.hpp>
#include <boost/thread/lock_types.hpp>

#include <regex>

namespace sw
{

Version Program::getVersion() const
{
    if (version)
        return version.value();

    if (file.empty())
    {
        version = gatherVersion();
        return version.value();
    }

    static std::unordered_map<path, Version> versions;
    static boost::upgrade_mutex m;

    boost::upgrade_lock lk(m);
    auto i = versions.find(file);
    if (i != versions.end())
    {
        version = i->second;
        return version.value();
    }

    boost::upgrade_to_unique_lock lk2(lk);

    if (version) // double check
        return version.value();

    version = versions[file] = gatherVersion();
    return version.value();
}

Version Program::gatherVersion(const path &program, const String &arg, const String &in_regex) const
{
    static std::regex r_default("(\\d+)\\.(\\d+)\\.(\\d+)(\\.(\\d+))?");

    std::regex r_in;
    if (!in_regex.empty())
        r_in.assign(in_regex);

    auto &r = in_regex.empty() ? r_default : r_in;

    Version V;
    builder::detail::ResolvableCommand c; // for nice program resolving
    c.program = program;
    c.args = { arg };
    error_code ec;
    c.execute(ec);

    std::smatch m;
    if (std::regex_search(c.err.text.empty() ? c.out.text : c.err.text, m, r))
    {
        if (m[5].matched)
            V = { std::stoi(m[1].str()), std::stoi(m[2].str()), std::stoi(m[3].str()), std::stoi(m[5].str()) };
        else
            V = { std::stoi(m[1].str()), std::stoi(m[2].str()), std::stoi(m[3].str()) };
    }
    return V;
}

}
