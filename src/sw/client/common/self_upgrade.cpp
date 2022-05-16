// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2017-2019 Egor Pugin <egor.pugin@gmail.com>

#include "self_upgrade.h"

#include "sig.h"

#include <sw/manager/settings.h>
#include <sw/manager/remote.h>

#include <boost/dll.hpp>
#include <primitives/pack.h>

#include <thread>

#ifdef _WIN32
#include <windows.h>
#endif

void self_upgrade(const String &progname)
{
#ifdef _WIN32
    path client = "/client/"s + progname + "-master-windows-client.zip";
#elif __APPLE__
    #ifdef __aarch64__
    path client = "/client/"s + progname + "-master-macos_arm64-client.tar.gz";
    #else
    path client = "/client/"s + progname + "-master-macos-client.tar.gz";
    #endif
#else
    path client = "/client/"s + progname + "-master-linux-client.tar.gz";
#endif

    auto &s = sw::Settings::get_user_settings();

    std::cout << "Downloading signature file" << "\n";
    static const auto algo = "sha512"s;
    auto &r = s.getRemotes(true)[0];
    auto sig = download_file(r->url + to_string(client.u8string()) + "." + algo + ".sig");

    auto fn = fs::temp_directory_path() / (unique_path() += client.extension());
    std::cout << "Downloading the latest client" << "\n";
    download_file(r->url + to_string(client.u8string()), fn, 100_MB);
    try
    {
        ds_verify_sw_file(fn, algo, sig);
    }
    catch (const std::exception &e)
    {
        throw std::runtime_error("Downloaded bad file (signature check failed): "s + e.what());
    }

    std::cout << "Unpacking" << "\n";
    auto tmp_dir = fs::temp_directory_path() / (progname + ".bak");
    unpack_file(fn, tmp_dir);
    fs::remove(fn);

    // self update
    auto program = path(boost::dll::program_location().wstring());
#ifdef _WIN32
    auto exe = (tmp_dir / (progname + ".exe")).wstring();
    auto arg0 = L"\"" + exe + L"\"";
    auto dst = L"\"" + program.wstring() + L"\"";
    std::cout << "Replacing client" << "\n";
    auto cmd_line = arg0 + L" -internal-self-upgrade-copy " + dst;
    STARTUPINFO si = { 0 };
    PROCESS_INFORMATION pi = { 0 };
    if (!CreateProcess(exe.c_str(), &cmd_line[0], 0, 0, 0, 0, 0, 0, &si, &pi))
    {
        throw std::runtime_error("errno = "s + std::to_string(errno) + "\n" +
            "Cannot do a self upgrade. Replace this file with newer SW client manually.");
    }
#else
    auto cppan = tmp_dir / progname;
    fs::permissions(cppan, fs::perms::owner_all | fs::perms::group_exec | fs::perms::others_exec);
    fs::remove(program);
    fs::copy_file(cppan, program);
    fs::remove(cppan);
#endif
}

void self_upgrade_copy(const path &dst)
{
    int n = 3;
    while (n--)
    {
        std::cout << "Waiting old program to exit...\n";
        std::this_thread::sleep_for(std::chrono::seconds(2));
        try
        {
            fs::copy_file(boost::dll::program_location().wstring(), dst, fs::copy_options::overwrite_existing);
            break;
        }
        catch (std::exception &e)
        {
            std::cerr << "Cannot replace program with new executable: " << e.what() << "\n";
            if (n == 0)
                throw;
            std::cerr << "Retrying... (" << n << ")\n";
        }
    }
    std::cout << "Success!\n";
}
