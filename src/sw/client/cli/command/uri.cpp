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

#include <sw/core/input.h>
#include <sw/manager/storage.h>
#include <sw/support/filesystem.h>

#include <primitives/http.h>
#include <primitives/sw/settings_program_name.h>

#ifdef _WIN32
#include <primitives/win32helpers.h>
#include <windows.h>
#include <shellapi.h>
#include <Objbase.h>
#include <Shlobj.h>
#endif

extern bool bUseSystemPause;
extern String gUploadPrefix;

DEFINE_SUBCOMMAND(uri, "Used to invoke sw application from the website.");

static ::cl::list<String> uri_args(::cl::Positional, ::cl::desc("sw uri arguments"), ::cl::sub(subcommand_uri));

#define F_ARGS sw::SwContext &swctx, sw::LocalStorage &sdb, const sw::LocalPackage &p
#ifdef _MSC_VER
#define F(n, ...) static void n(F_ARGS, __VA_ARGS__)
#else
#define F(n, ...) static void n(F_ARGS, ##__VA_ARGS__)
#endif

F(open_dir, const path &d)
{
#ifdef _WIN32
    if (sdb.isPackageInstalled(p))
    {
        auto pidl = ILCreateFromPath(d.wstring().c_str());
        if (pidl)
        {
            CoInitialize(0);
            // ShellExecute does not work here for some scenarios
            auto r = SHOpenFolderAndSelectItems(pidl, 0, 0, 0);
            if (FAILED(r))
            {
                message_box(sw::getProgramName(), "Error in SHOpenFolderAndSelectItems");
            }
            ILFree(pidl);
        }
        else
        {
            message_box(sw::getProgramName(), "Error in ILCreateFromPath");
        }
    }
    else
    {
        message_box(sw::getProgramName(), "Package '" + p.toString() + "' is not installed");
    }
#endif
}

F(open_file, const path &f)
{
#ifdef _WIN32
    if (sdb.isPackageInstalled(p))
    {
        CoInitialize(0);
        auto r = ShellExecute(0, L"open", f.wstring().c_str(), 0, 0, 0);
        if (r <= (HINSTANCE)HINSTANCE_ERROR)
        {
            message_box(sw::getProgramName(), "Error in ShellExecute");
        }
    }
    else
    {
        message_box(sw::getProgramName(), "Package '" + p.toString() + "' is not installed");
    }
#endif
}

F(install)
{
#ifdef _WIN32
    if (!sdb.isPackageInstalled(p))
    {
        SetupConsole();
        bUseSystemPause = true;
        swctx.install(sw::UnresolvedPackages{ sw::UnresolvedPackage{p.getPath(), p.getVersion()} });
    }
    else
    {
        message_box(sw::getProgramName(), "Package '" + p.toString() + "' is already installed");
    }
#endif
}

F(remove)
{
    p.remove();
}

F(build)
{
#ifdef _WIN32
    SetupConsole();
    bUseSystemPause = true;
#endif
    auto d = swctx.getLocalStorage().storage_dir_tmp / "build";// / fs::unique_path();
    fs::create_directories(d);
    ScopedCurrentPath scp(d, CurrentPathScope::All);
    auto b = swctx.createBuild();
    sw::InputWithSettings i(swctx.addInput(p));
    b->addInput(i);
    b->build();
}

F(run)
{
#ifdef _WIN32
    SetupConsole();
    bUseSystemPause = true;
#endif
    auto d = swctx.getLocalStorage().storage_dir_tmp / "build";// / fs::unique_path();
    fs::create_directories(d);
    ScopedCurrentPath scp(d, CurrentPathScope::All);
    SW_UNIMPLEMENTED;
    //sw::run(swctx, p);
}

F(upload)
{
    if (uri_args.size() != 4)
        throw SW_RUNTIME_ERROR("Bad upload args");

    auto rs = swctx.getRemoteStorages();
    if (rs.empty())
        throw SW_RUNTIME_ERROR("No remote storages found");

    sw::Package pkg(*rs.front(), uri_args[1]);
    sw::Version new_version(uri_args[2]);

    String url = "https://raw.githubusercontent.com/SoftwareNetwork/specifications/master/";
    url += normalize_path(pkg.getHashPath() / "sw.cpp");
    auto fn = sw::get_temp_filename("uploads") / "sw.cpp";
    auto spec_data = download_file(url);
    boost::replace_all(spec_data, pkg.getVersion().toString(), new_version.toString());
    write_file(fn, spec_data);

    // before scp
    SCOPE_EXIT
    {
        // free files
        swctx.clearFileStorages();
        fs::remove_all(fn.parent_path());
    };

    // run secure as below?
    ScopedCurrentPath scp(fn.parent_path());
    gUploadPrefix = pkg.getPath().slice(0, std::stoi(uri_args[3]));
    cli_upload(swctx);

    /*primitives::Command c;
    c.program = "sw";
    c.working_directory = fn.parent_path();
    c.args.push_back("upload");
    c.args.push_back(pkg.ppath.slice(0, std::stoi(uri_args[3])));
    c.out.inherit = true;
    c.err.inherit = true;
    //c.execute();*/
}

static void dispatcher()
{
    auto swctx = createSwContext();
    auto id = sw::extractPackageIdFromString(uri_args[1]);
    auto &sdb = swctx->getLocalStorage();
    sw::LocalPackage p(sdb, id);

#ifdef _MSC_VER
#define URI_CMD2(x, f, ...)             \
    if (uri_args[0] == "sw:" #x)        \
    {                                   \
        f(*swctx, sdb, p, __VA_ARGS__); \
        return;                         \
    }
#define URI_CMD(x, ...) \
    URI_CMD2(x, x, __VA_ARGS__)
#else
#define URI_CMD2(x, f, ...)               \
    if (uri_args[0] == "sw:" #x)          \
    {                                     \
        f(*swctx, sdb, p, ##__VA_ARGS__); \
        return;                           \
    }
#define URI_CMD(x, ...) \
    URI_CMD2(x, x, ##__VA_ARGS__)
#endif

    URI_CMD2(sdir, open_dir, p.getDirSrc2());
    URI_CMD2(bdir, open_dir, p.getDirObj());
    URI_CMD2(open_build_script, open_file, p.getDirSrc2() / "sw.cpp");
    URI_CMD(install);
    URI_CMD(remove);
    URI_CMD(build);
    URI_CMD(run);
    URI_CMD(upload);

    throw SW_RUNTIME_ERROR("Unknown command: " + uri_args[0]);
}

SUBCOMMAND_DECL(uri)
{
    fs::current_path(sw::temp_directory_path());

    if (uri_args.size() <= 1)
        return;

    try
    {
        dispatcher();
    }
    catch (std::exception &e)
    {
#ifdef _WIN32
        message_box(sw::getProgramName(), e.what());
#endif
    }
    catch (...)
    {
#ifdef _WIN32
        message_box(sw::getProgramName(), "Unknown exception");
#endif
    }
}
