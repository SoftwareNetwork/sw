// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2017-2019 Egor Pugin <egor.pugin@gmail.com>

#include "../commands.h"

#include <sw/manager/storage.h>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#include <Objbase.h>
#include <Shlobj.h>
#endif

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "command.open");

static void open_nix(const String &p)
{
#ifdef _WIN32
    SW_UNREACHABLE;
#endif
    String s;
#ifdef __linux__
    s += "xdg-";
#endif
    s += "open \"" + to_string(normalize_path(p)) + "\"";
    if (system(s.c_str()) != 0)
    {
#if !(defined(__linux__) || defined(__APPLE__))
        SW_UNIMPLEMENTED;
#else
        throw SW_RUNTIME_ERROR("Cannot open: " + p);
#endif
    }
}

void open_directory(const path &p)
{
#ifdef _WIN32
    auto pidl = ILCreateFromPath(p.wstring().c_str());
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
#else
    open_nix(p);
#endif
}

void open_file(const path &p)
{
#ifdef _WIN32
    CoInitialize(0);
    auto r = ShellExecute(0, L"open", p.wstring().c_str(), 0, 0, 0);
    if (r <= (HINSTANCE)HINSTANCE_ERROR)
    {
        throw SW_RUNTIME_ERROR("Error in ShellExecute");
    }
#else
    open_nix(p);
#endif
}

void open_url(const String &url)
{
#ifdef _WIN32
    CoInitialize(0);
    auto r = ShellExecute(0, L"open", to_wstring(url).c_str(), 0, 0, 0);
    if (r <= (HINSTANCE)HINSTANCE_ERROR)
    {
        throw SW_RUNTIME_ERROR("Error in ShellExecute");
    }
#else
    open_nix(url);
#endif
}

SUBCOMMAND_DECL(open)
{
    auto &sdb = getContext().getLocalStorage();

    sw::UnresolvedPackages upkgs;
    for (auto &a : getInputs())
        upkgs.insert(a);

    SW_UNIMPLEMENTED;
    /*auto pkgs = getContext().resolve(upkgs);
    for (auto &u : upkgs)
    {
        auto ip = pkgs.find(u);
        if (ip == pkgs.end())
        {
            LOG_WARN(logger, "Cannot get " + u.toString());
            continue;
        }
        auto &p = *ip->second;
        if (!sdb.isPackageInstalled(p))
        {
            LOG_INFO(logger, "Package '" + p.toString() + "' not installed");
            continue;
        }

        sw::LocalPackage lp(getContext().getLocalStorage(), p);

        LOG_INFO(logger, "package: " + lp.toString());
        LOG_INFO(logger, "package dir: " + to_string(lp.getDir().u8string()));

        open_directory(lp.getDirSrc() / ""); // on win we must add last slash
    }*/
}
