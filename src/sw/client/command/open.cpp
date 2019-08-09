/*
 * SW - Build System and Package Manager
 * Copyright (C) 2017-2019 Egor Pugin
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "commands.h"

#include <sw/manager/storage.h>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#include <Objbase.h>
#include <Shlobj.h>
#endif

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "command.open");

DEFINE_SUBCOMMAND(open, "Open package directory.");

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
