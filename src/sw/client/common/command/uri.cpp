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

#define F_ARGS std::unique_ptr<sw::SwContext> swctx, sw::LocalStorage &sdb, const sw::LocalPackage &p, OPTIONS_ARG
#ifdef _MSC_VER
#define F(n, ...) static void n(F_ARGS, __VA_ARGS__)
#else
#define F(n, ...) static void n(F_ARGS, ##__VA_ARGS__)
#endif

static void setup_console()
{
#ifdef _WIN32
    //SetupConsole();
    //bUseSystemPause = true;
#endif
}

void open_directory(const path &);
void open_file(const path &);

F(open_dir, const path &d)
{
    if (!sdb.isPackageInstalled(p))
        throw SW_RUNTIME_ERROR("Package '" + p.toString() + "' is not installed");
    open_directory(d);
}

F(open_file, const path &f)
{
    if (!sdb.isPackageInstalled(p))
        throw SW_RUNTIME_ERROR("Package '" + p.toString() + "' is not installed");
    open_file(f);
}

F(install)
{
    if (sdb.isPackageInstalled(p))
        throw SW_RUNTIME_ERROR("Package '" + p.toString() + "' is already installed");
    setup_console();
    swctx->install(sw::UnresolvedPackages{ sw::UnresolvedPackage{p.getPath(), p.getVersion()} });
}

F(remove)
{
    p.remove();
}

F(build)
{
    setup_console();
    auto d = swctx->getLocalStorage().storage_dir_tmp / "build";// / fs::unique_path();
    fs::create_directories(d);
    ScopedCurrentPath scp(d, CurrentPathScope::All);
    auto b = swctx->createBuild();
    sw::InputWithSettings i(swctx->addInput(p));
    b->addInput(i);
    b->build();
}

F(run)
{
    setup_console();

    // simple protection for now
    if (p.getPath().isRelative() || p.getPath().getOwner() != "sw")
        return;

    auto d = swctx->getLocalStorage().storage_dir_tmp / "build";// / fs::unique_path();
    fs::create_directories(d);
    ScopedCurrentPath scp(d, CurrentPathScope::All);

    primitives::Command c;

    // set flags always
    c.create_new_console = true;
    // detach is needed because only it helps spawned program to outlive sw app
    c.detached = true;

    run(*swctx, p, c, options);
}

F(upload)
{
    if (options.options_uri.uri_args.size() != 4)
        throw SW_RUNTIME_ERROR("Bad upload args");

    auto rs = swctx->getRemoteStorages();
    if (rs.empty())
        throw SW_RUNTIME_ERROR("No remote storages found");

    sw::Package pkg(*rs.front(), options.options_uri.uri_args[1]);
    sw::Version new_version(options.options_uri.uri_args[2]);

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
        swctx.reset();
        fs::remove_all(fn.parent_path());
    };

    // run secure as below?
    ScopedCurrentPath scp(fn.parent_path());
    options.options_upload.upload_prefix = pkg.getPath().slice(0, std::stoi(options.options_uri.uri_args[3]));
    cli_upload(*swctx, options);

    /*primitives::Command c;
    c.program = "sw";
    c.working_directory = fn.parent_path();
    c.args.push_back("upload");
    c.args.push_back(pkg.ppath.slice(0, std::stoi(uri_args[3])));
    c.out.inherit = true;
    c.err.inherit = true;
    //c.execute();*/
}

static void dispatcher(OPTIONS_ARG)
{
    auto swctx = createSwContext(options);
    auto id = sw::extractPackageIdFromString(options.options_uri.uri_args[1]);
    auto &sdb = swctx->getLocalStorage();
    sw::LocalPackage p(sdb, id);

#ifdef _MSC_VER
#define URI_CMD2(x, f, ...)                                \
    if (options.options_uri.uri_args[0] == "sw:" #x)       \
    {                                                      \
        f(std::move(swctx), sdb, p, options, __VA_ARGS__); \
        return;                                            \
    }
#define URI_CMD(x, ...) \
    URI_CMD2(x, x, __VA_ARGS__)
#else
#define URI_CMD2(x, f, ...)                                  \
    if (options.options_uri.uri_args[0] == "sw:" #x)         \
    {                                                        \
        f(std::move(swctx), sdb, p, options, ##__VA_ARGS__); \
        return;                                              \
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

    throw SW_RUNTIME_ERROR("Unknown command: " + options.options_uri.uri_args[0]);
}

SUBCOMMAND_DECL(uri)
{
    fs::current_path(sw::temp_directory_path());

    if (options.options_uri.uri_args.size() <= 1)
        return;

    try
    {
        dispatcher(options);
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
