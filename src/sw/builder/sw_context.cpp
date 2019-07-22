// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "sw_context.h"

#include "command_storage.h"
#include "file_storage.h"
#include "program_version_storage.h"

#include <sw/manager/storage.h>

#include <boost/thread/lock_types.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <primitives/executor.h>

#include <regex>

namespace sw
{

SwBuilderContext::SwBuilderContext(const path &local_storage_root_dir)
    : SwManagerContext(local_storage_root_dir)
{
    HostOS = getHostOS();

    //
    file_storage_executor = std::make_unique<Executor>("async log writer", 1);

    //
    pvs = std::make_unique<ProgramVersionStorage>(getLocalStorage().storage_dir_tmp / "db" / "program_versions.txt");
}

SwBuilderContext::~SwBuilderContext()
{
}

ModuleStorage &SwBuilderContext::getModuleStorage() const
{
    return *module_storage;
}

Executor &SwBuilderContext::getFileStorageExecutor() const
{
    return *file_storage_executor;
}

FileStorage &SwBuilderContext::getFileStorage() const
{
    if (!file_storage)
        file_storage = std::make_unique<FileStorage>();
    return *file_storage;
}

CommandStorage &SwBuilderContext::getCommandStorage() const
{
    if (!cs)
    {
        std::unique_lock lk(csm);
        if (!cs)
            cs = std::make_unique<CommandStorage>(*this);
    }
    return *cs;
}

ProgramVersionStorage &SwBuilderContext::getVersionStorage() const
{
    return *pvs;
}

void SwBuilderContext::clearFileStorages()
{
    file_storage.reset();
}

static Version gatherVersion1(builder::detail::ResolvableCommand &c, const String &in_regex)
{
    error_code ec;
    c.execute(ec);

    if (c.pid == -1)
        throw SW_RUNTIME_ERROR(normalize_path(c.getProgram()) + ": " + ec.message());

    static std::regex r_default("(\\d+)\\.(\\d+)\\.(\\d+)(\\.(\\d+))?");

    std::regex r_in;
    if (!in_regex.empty())
        r_in.assign(in_regex);

    auto &r = in_regex.empty() ? r_default : r_in;

    Version V;
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

static Version gatherVersion(const path &program, const String &arg, const String &in_regex)
{
    builder::detail::ResolvableCommand c; // for nice program resolving
    c.setProgram(program);
    if (!arg.empty())
        c.push_back(arg);
    return gatherVersion1(c, in_regex);
}

Version getVersion(const SwBuilderContext &swctx, builder::detail::ResolvableCommand &c, const String &in_regex)
{
    auto &vs = swctx.getVersionStorage();
    static boost::upgrade_mutex m;

    const auto program = c.getProgram();

    boost::upgrade_lock lk(m);
    auto i = vs.versions.find(program);
    if (i != vs.versions.end())
        return i->second;

    boost::upgrade_to_unique_lock lk2(lk);

    vs.versions[program] = gatherVersion1(c, in_regex);
    return vs.versions[program];
}

Version getVersion(const SwBuilderContext &swctx, const path &program, const String &arg, const String &in_regex)
{
    auto &vs = swctx.getVersionStorage();
    static boost::upgrade_mutex m;

    boost::upgrade_lock lk(m);
    auto i = vs.versions.find(program);
    if (i != vs.versions.end())
        return i->second;

    boost::upgrade_to_unique_lock lk2(lk);

    vs.versions[program] = gatherVersion(program, arg, in_regex);
    return vs.versions[program];
}

}

