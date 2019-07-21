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

#include <sw/support/filesystem.h>
#include <primitives/http.h>
#include <primitives/sw/settings_program_name.h>
#include <primitives/win32helpers.h>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#include <Objbase.h>
#include <Shlobj.h>
#endif

extern bool bUseSystemPause;
extern String gUploadPrefix;

static ::cl::list<String> uri_args(::cl::Positional, ::cl::desc("sw uri arguments"), ::cl::sub(subcommand_uri));
//static ::cl::opt<String> uri_sdir("sw:sdir", ::cl::desc("Open source dir in file browser"), ::cl::sub(subcommand_uri));

SUBCOMMAND_DECL(uri)
{
    fs::current_path(sw::temp_directory_path());

    if (uri_args.empty())
        return;
    if (uri_args.size() == 1)
        return;

    try
    {
        auto swctx = createSwContext();
        auto id = sw::extractPackageIdFromString(uri_args[1]);
        auto &sdb = swctx->getLocalStorage();
        sw::LocalPackage p(sdb, id);

        if (uri_args[0] == "sw:sdir" || uri_args[0] == "sw:bdir")
        {
#ifdef _WIN32
            if (sdb.isPackageInstalled(p))
            {
                auto pidl = uri_args[0] == "sw:sdir" ?
                    ILCreateFromPath(p.getDirSrc2().wstring().c_str()) :
                    ILCreateFromPath(p.getDirObj().wstring().c_str())
                    ;
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
                message_box(sw::getProgramName(), "Package '" + p.toString() + "' not installed");
            }
#endif
            return;
        }

        if (uri_args[0] == "sw:open_build_script")
        {
#ifdef _WIN32
            if (sdb.isPackageInstalled(p))
            {
                auto f = (p.getDirSrc2() / "sw.cpp").wstring();

                CoInitialize(0);
                auto r = ShellExecute(0, L"open", f.c_str(), 0, 0, 0);
                if (r <= (HINSTANCE)HINSTANCE_ERROR)
                {
                    message_box(sw::getProgramName(), "Error in ShellExecute");
                }
            }
            else
            {
                message_box(sw::getProgramName(), "Package '" + p.toString() + "' not installed");
            }
#endif
            return;
        }

        if (uri_args[0] == "sw:install")
        {
#ifdef _WIN32
            if (!sdb.isPackageInstalled(p))
            {
                SetupConsole();
                bUseSystemPause = true;
                swctx->install(sw::UnresolvedPackages{ sw::UnresolvedPackage{p.ppath, p.version} });
            }
            else
            {
                message_box(sw::getProgramName(), "Package '" + p.toString() + "' is already installed");
            }
#endif
            return;
        }

        if (uri_args[0] == "sw:remove")
        {
            SW_UNIMPLEMENTED;

            //sdb.removeInstalledPackage(p); // TODO: remove from db
            error_code ec;
            fs::remove_all(p.getDir(), ec);
            return;
        }

        if (uri_args[0] == "sw:build")
        {
#ifdef _WIN32
            SetupConsole();
            bUseSystemPause = true;
#endif
            auto d = swctx->getLocalStorage().storage_dir_tmp / "build";// / fs::unique_path();
            fs::create_directories(d);
            ScopedCurrentPath scp(d, CurrentPathScope::All);
            auto b = swctx->createBuild();
            b.addInput(p);
            b.build();
            return;
        }

        if (uri_args[0] == "sw:run")
        {
#ifdef _WIN32
            SetupConsole();
            bUseSystemPause = true;
#endif
            auto d = swctx->getLocalStorage().storage_dir_tmp / "build";// / fs::unique_path();
            fs::create_directories(d);
            ScopedCurrentPath scp(d, CurrentPathScope::All);
            SW_UNIMPLEMENTED;
            //sw::run(swctx, p);
            return;
        }

        if (uri_args[0] == "sw:upload")
        {
            if (uri_args.size() != 4)
                return;

            auto rs = swctx->getRemoteStorages();
            if (rs.empty())
                throw SW_RUNTIME_ERROR("No remote storages found");

            sw::Package pkg(*rs.front(), uri_args[1]);
            sw::Version new_version(uri_args[2]);

            String url = "https://raw.githubusercontent.com/SoftwareNetwork/specifications/master/";
            url += normalize_path(pkg.getHashPath() / "sw.cpp");
            auto fn = sw::get_temp_filename("uploads") / "sw.cpp";
            auto spec_data = download_file(url);
            boost::replace_all(spec_data, pkg.version.toString(), new_version.toString());
            write_file(fn, spec_data);

            // before scp
            SCOPE_EXIT
            {
                // free files
                swctx->clearFileStorages();
            fs::remove_all(fn.parent_path());
            };

            // run secure as below?
            ScopedCurrentPath scp(fn.parent_path());
            gUploadPrefix = pkg.ppath.slice(0, std::stoi(uri_args[3]));
            cli_upload(*swctx);

            /*primitives::Command c;
            c.program = "sw";
            c.working_directory = fn.parent_path();
            c.args.push_back("upload");
            c.args.push_back(pkg.ppath.slice(0, std::stoi(uri_args[3])));
            c.out.inherit = true;
            c.err.inherit = true;
            //c.execute();*/

            return;
        }

        throw SW_RUNTIME_ERROR("Unknown command: " + uri_args[0]);
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
