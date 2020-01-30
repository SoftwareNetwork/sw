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

//#include <primitives/log.h>
//DECLARE_STATIC_LOGGER(logger, "builder.context");

namespace sw
{

SwBuilderContext::SwBuilderContext(const path &local_storage_root_dir)
    : SwManagerContext(local_storage_root_dir)
{
#ifdef _WIN32
    // with per pkg command log we must increase the limits
    //auto new_limit = 8192;
    //if (_setmaxstdio(new_limit) == -1)
        //LOG_ERROR(logger, "Cannot raise number of maximum opened files");
#endif

    HostOS = getHostOS();

    module_storage = std::make_unique<ModuleStorage>();

    //
    file_storage_executor = std::make_unique<Executor>("async log writer", 1);

    //
    pvs = std::make_unique<ProgramVersionStorage>(getLocalStorage().storage_dir_tmp / "db" / "program_versions.txt");
}

SwBuilderContext::~SwBuilderContext()
{
    // do not clear modules on exception, because it may come from there
    // TODO: cleanup modules data first
    // copy exception here and pass further?
    //if (std::uncaught_exceptions())
        //module_storage.release();
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

CommandStorage &SwBuilderContext::getCommandStorage(const path &root) const
{
    std::unique_lock lk(csm);
    auto &cs = command_storages[root];
    if (!cs)
        cs = std::make_unique<CommandStorage>(*this, root);
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

    static std::regex r_default("(\\d+)(\\.(\\d+)){2,}(-[[:alnum:]]+([.-][[:alnum:]]+)*)?");

    std::regex r_in;
    if (!in_regex.empty())
        r_in.assign(in_regex);

    auto &r = in_regex.empty() ? r_default : r_in;

    Version V;
    std::smatch m;
    if (std::regex_search(c.err.text.empty() ? c.out.text : c.err.text, m, r))
    {
        auto s = m[0].str();
        if (m[4].matched)
        {
            // some programs write extra as 'beta2-123-123' when we expect 'beta2.123.123'
            // this math skips until m[4] started plus first '-'
            std::replace(s.begin() + (m[4].first - m[0].first) + 1, s.end(), '-', '.');
        }
        V = s;
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

    vs.addVersion(program, gatherVersion1(c, in_regex));
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

    vs.addVersion(program, gatherVersion(program, arg, in_regex));
    return vs.versions[program];
}

}

