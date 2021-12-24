// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2017-2019 Egor Pugin <egor.pugin@gmail.com>

#include "../commands.h"

#include <sw/core/input.h>
#include <sw/manager/storage.h>
#include <sw/support/filesystem.h>

#include <boost/algorithm/string.hpp>
#include <primitives/http.h>
#include <primitives/sw/settings_program_name.h>

#include <iostream>

#ifdef _WIN32
#include <primitives/win32helpers.h>
#include <windows.h>
#include <shellapi.h>
#include <Objbase.h>
#include <Shlobj.h>
#endif

extern bool bUseSystemPause;

#define F_ARGS SwClientContext &swctx, sw::LocalStorage &sdb, const sw::LocalPackage &p
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

static void free_ctx_and_delete_files(SwClientContext &swctx, const path &d)
{
    // free files
    swctx.resetContext();
    fs::remove_all(d);
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
#ifdef __linux__
    // sometimes we need more time to process file open
    // otherwise our process terminates and it terminates child call chain
    std::this_thread::sleep_for(std::chrono::seconds(3));
#endif
}

F(install)
{
    if (sdb.isPackageInstalled(p))
        throw SW_RUNTIME_ERROR("Package '" + p.toString() + "' is already installed");
    setup_console();
    swctx.getContext().install(sw::UnresolvedPackages{ p });
}

F(remove)
{
    p.remove();
}

F(build)
{
    setup_console();

    // simple protection for now
    if (p.getPath().isRelative() || p.getPath().getOwner() != "sw")
        throw SW_RUNTIME_ERROR("Insecure operation. Aborting...");

    swctx.getContext().install(sw::UnresolvedPackages{ p });
    auto d = swctx.getContext().getLocalStorage().storage_dir_tmp / "build" / unique_path();
    fs::create_directories(d);
    SCOPE_EXIT
    {
        free_ctx_and_delete_files(swctx, d);
    };
    ScopedCurrentPath scp(d, CurrentPathScope::All);
    auto b = swctx.createBuildAndPrepare({ p.toString() });
    auto i = b->addInput(p);
    sw::InputWithSettings ii(i);
    b->addInput(ii);
    b->build();
}

F(run)
{
    setup_console();

    // simple protection for now
    if (p.getPath().isRelative() || p.getPath().getOwner() != "sw")
        throw SW_RUNTIME_ERROR("Insecure operation. Aborting...");

    swctx.getContext().install(sw::UnresolvedPackages{ p });
    auto d = swctx.getContext().getLocalStorage().storage_dir_tmp / "build" / unique_path();
    fs::create_directories(d);
    SCOPE_EXIT
    {
        free_ctx_and_delete_files(swctx, d);
    };
    ScopedCurrentPath scp(d, CurrentPathScope::All);

    primitives::Command c;

    // set flags always
    c.create_new_console = true;
    // detach is needed because only it helps spawned program to outlive sw app
    c.detached = true;

    swctx.run(p, c);
}

F(upload)
{
    if (swctx.getOptions().options_uri.uri_args.size() != 4)
        throw SW_RUNTIME_ERROR("Bad upload args");

    auto rs = swctx.getContext().getRemoteStorages();
    if (rs.empty())
        throw SW_RUNTIME_ERROR("No remote storages found");

    sw::Package pkg(*rs.front(), swctx.getOptions().options_uri.uri_args[1]);
    sw::Version new_version(swctx.getOptions().options_uri.uri_args[2]);

    String url = "https://raw.githubusercontent.com/SoftwareNetwork/specifications/master/";
    url += to_string(normalize_path(pkg.getHashPath() / "sw.cpp"));
    auto fn = sw::support::get_temp_filename("uploads") / "sw.cpp";
    auto spec_data = download_file(url);
    boost::replace_all(spec_data, pkg.getVersion().toString(), new_version.toString());
    write_file(fn, spec_data);

    // before scp
    SCOPE_EXIT
    {
        free_ctx_and_delete_files(swctx, fn.parent_path());
    };

    // run secure as below?
    ScopedCurrentPath scp(fn.parent_path());
    swctx.getOptions().options_upload.upload_prefix = pkg.getPath().slice(0, std::stoi(swctx.getOptions().options_uri.uri_args[3]));
    swctx.command_upload();

    /*primitives::Command c;
    c.program = "sw";
    c.working_directory = fn.parent_path();
    c.args.push_back("upload");
    c.args.push_back(pkg.ppath.slice(0, std::stoi(uri_args[3])));
    c.out.inherit = true;
    c.err.inherit = true;
    //c.execute();*/
}

static void dispatcher(SwClientContext &swctx)
{
    auto id = sw::extractPackageIdFromString(swctx.getOptions().options_uri.uri_args[1]);
    auto &sdb = swctx.getContext().getLocalStorage();
    sw::LocalPackage p(sdb, id);

#ifdef _MSC_VER
#define URI_CMD2(x, f, ...)                                     \
    if (swctx.getOptions().options_uri.uri_args[0] == "sw:" #x) \
    {                                                           \
        f(swctx, sdb, p, __VA_ARGS__);                          \
        return;                                                 \
    }
#define URI_CMD(x, ...) \
    URI_CMD2(x, x, __VA_ARGS__)
#else
#define URI_CMD2(x, f, ...)                                     \
    if (swctx.getOptions().options_uri.uri_args[0] == "sw:" #x) \
    {                                                           \
        f(swctx, sdb, p, ##__VA_ARGS__);                        \
        return;                                                 \
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

    throw SW_RUNTIME_ERROR("Unknown command: " + swctx.getOptions().options_uri.uri_args[0]);
}

SUBCOMMAND_DECL(uri)
{
    fs::current_path(sw::support::temp_directory_path());

    if (getOptions().options_uri.uri_args.size() <= 1)
        return;

    try
    {
        dispatcher(*this);
    }
    catch (std::exception &e)
    {
#ifdef _WIN32
        message_box(sw::getProgramName(), e.what());
#else
        std::cerr << e.what();
        std::cerr << "\nPress any key to continue...";
        getchar();
#endif
    }
    catch (...)
    {
#ifdef _WIN32
        message_box(sw::getProgramName(), "Unknown exception");
#else
        std::cerr << "unknown exception";
        std::cerr << "\nPress any key to continue...";
        getchar();
#endif
    }
}
