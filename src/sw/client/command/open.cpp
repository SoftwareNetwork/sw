// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "commands.h"

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#include <Objbase.h>
#include <Shlobj.h>
#endif

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "command.open");

static ::cl::opt<String> open_arg(::cl::Positional, ::cl::desc("package to open"), ::cl::sub(subcommand_open));

SUBCOMMAND_DECL(open)
{
    auto swctx = createSwContext();
    auto &sdb = swctx->getLocalStorage();
    sw::LocalPackage p(swctx->getLocalStorage(), open_arg);

#ifdef _WIN32
    if (sdb.isPackageInstalled(p))
    {
        LOG_INFO(logger, "package: " + p.toString());
        LOG_INFO(logger, "package dir: " + p.getDir().u8string());

        auto pidl = ILCreateFromPath((p.getDirSrc() / "").wstring().c_str());
        if (pidl)
        {
            CoInitialize(0);
            // ShellExecute does not work here for some scenarios
            auto r = SHOpenFolderAndSelectItems(pidl, 0, 0, 0);
            if (FAILED(r))
            {
                LOG_INFO(logger, "Error in SHOpenFolderAndSelectItems");
            }
            ILFree(pidl);
        }
        else
        {
            LOG_INFO(logger, "Error in ILCreateFromPath");
        }
    }
    else
    {
        LOG_INFO(logger, "Package '" + p.toString() + "' not installed");
    }
#endif
}
