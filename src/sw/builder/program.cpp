// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "program.h"

#include "command.h"
#include "file_storage.h"
#include "program_version_storage.h"
#include "sw_context.h"

#include <sw/manager/storage.h>

#include <boost/thread/shared_mutex.hpp>
#include <boost/thread/lock_types.hpp>

#include <fstream>
#include <regex>

namespace sw
{

Program::Program(const SwContext &swctx)
    : swctx(swctx)
{
}

Program::Program(const Program &rhs)
    : File(rhs), swctx(rhs.swctx)
{
}

/*Program &Program::operator=(const Program &rhs)
{
    //swctx = rhs.swctx;
    return *this;
}*/

const Version &Program::getVersion() const
{
    return const_cast<Program&>(*this).getVersion();
}

Version &Program::getVersion()
{
    if (version)
        return version.value();

    if (file.empty())
    {
        version = gatherVersion();
        return version.value();
    }

    auto &vs = swctx.getVersionStorage();
    static boost::upgrade_mutex m;

    boost::upgrade_lock lk(m);
    auto i = vs.versions.find(file);
    if (i != vs.versions.end())
    {
        version = i->second;
        return version.value();
    }

    boost::upgrade_to_unique_lock lk2(lk);

    if (version) // double check
        return version.value();

    version = vs.versions[file] = gatherVersion();
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
