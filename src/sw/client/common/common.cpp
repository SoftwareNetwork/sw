/*
 * SW - Build System and Package Manager
 * Copyright (C) 2017-2020 Egor Pugin
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

#include "common.h"

#include <sw/driver/driver.h>
#include <sw/manager/settings.h>

#include <primitives/emitter.h>
#include <primitives/http.h>
#include <primitives/sw/cl.h>

static ::cl::opt<path> storage_dir_override("storage-dir");

static ::cl::opt<bool> curl_verbose("curl-verbose");
static ::cl::opt<bool> ignore_ssl_checks("ignore-ssl-checks");

// from libs
extern String default_remote;
static cl::opt<String, true> cl_default_remote("r", cl::desc("Select default remote"), ::cl::location(default_remote));

extern bool allow_cygwin_hosts;
static cl::opt<bool, true> cl_allow_cygwin_hosts("host-cygwin", cl::desc("When on cygwin, allow it as host"), ::cl::location(allow_cygwin_hosts));

extern bool do_not_remove_bad_module;
static cl::opt<bool, true> cl_do_not_remove_bad_module("do-not-remove-bad-module", ::cl::location(do_not_remove_bad_module));

//
extern bool save_failed_commands;
extern bool save_all_commands;
extern bool save_executed_commands;

extern bool explain_outdated;
extern bool explain_outdated_full;

extern String save_command_format;

static cl::opt<bool, true> cl_save_failed_commands("save-failed-commands", cl::location(save_failed_commands));
static cl::alias sfc("sfc", cl::desc("Alias for -save-failed-commands"), cl::aliasopt(cl_save_failed_commands));

static cl::opt<bool, true> cl_save_all_commands("save-all-commands", cl::location(save_all_commands));
static cl::alias sac("sac", cl::desc("Alias for -save-all-commands"), cl::aliasopt(cl_save_all_commands));

static cl::opt<bool, true> cl_save_executed_commands("save-executed-commands", cl::location(save_executed_commands));
static cl::alias sec("sec", cl::desc("Alias for -save-executed-commands"), cl::aliasopt(cl_save_executed_commands));

//
static cl::opt<bool, true> cl_explain_outdated("explain-outdated", cl::desc("Explain outdated commands"), cl::location(explain_outdated));
static cl::opt<bool, true> cl_explain_outdated_full("explain-outdated-full", cl::desc("Explain outdated commands with more info"), cl::location(explain_outdated_full));

static cl::opt<String, true> cl_save_command_format("save-command-format", cl::desc("Explicitly set saved command format (bat or sh)"), cl::location(save_command_format));

//
extern bool debug_configs;
static cl::opt<bool, true> cl_debug_configs("debug-configs", cl::desc("Build configs in debug mode"), cl::location(debug_configs));

extern bool ignore_source_files_errors;
static cl::opt<bool, true> cl_ignore_source_files_errors("ignore-source-files-errors", cl::desc("Useful for debugging"), cl::location(ignore_source_files_errors));

extern bool do_not_mangle_object_names;
static cl::opt<bool, true> cl_do_not_mangle_object_names("do-not-mangle-object-names", cl::location(do_not_mangle_object_names));

extern bool standalone;
static cl::opt<bool, true> cl_standalone("standalone", cl::desc("Build standalone binaries"), cl::location(standalone), cl::init(true));
static cl::alias standalone2("sa", cl::aliasopt(cl_standalone));

//
extern bool checks_single_thread;
extern bool print_checks;
extern bool wait_for_cc_checks;
extern String cc_checks_command;

static cl::opt<bool, true> cl_checks_single_thread("checks-st", cl::desc("Perform checks in one thread (for cc)"), cl::location(checks_single_thread));
static cl::opt<bool, true> cl_print_checks("print-checks", cl::desc("Save extended checks info to file"), cl::location(print_checks));
static cl::opt<bool, true> cl_wait_for_cc_checks("wait-for-cc-checks", cl::desc("Do not exit on missing cc checks, wait for user input"), cl::location(wait_for_cc_checks));
static cl::opt<String, true> cl_cc_checks_command("cc-checks-command", cl::desc("Automatically execute cc checks command"), cl::location(cc_checks_command));
//

std::unique_ptr<sw::SwContext> createSwContext()
{
    // load proxy settings early
    httpSettings.verbose = curl_verbose;
    httpSettings.ignore_ssl_checks = ignore_ssl_checks;
    httpSettings.proxy = sw::Settings::get_local_settings().proxy;

    auto swctx = std::make_unique<sw::SwContext>(storage_dir_override.empty() ? sw::Settings::get_user_settings().storage_dir : storage_dir_override);
    // TODO:
    // before default?
    //for (auto &d : drivers)
    //swctx->registerDriver(std::make_unique<sw::driver::cpp::Driver>());
    swctx->registerDriver("org.sw.sw.driver.cpp-0.4.0"s, std::make_unique<sw::driver::cpp::Driver>());
    //swctx->registerDriver(std::make_unique<sw::CDriver>(sw_create_driver));
    return swctx;
}

String list_predefined_targets(sw::SwContext &swctx)
{
    using OrderedTargetMap = sw::PackageVersionMapBase<sw::TargetContainer, std::map, primitives::version::VersionMap>;

    OrderedTargetMap m;
    for (auto &[pkg, tgts] : swctx.getPredefinedTargets())
        m[pkg] = tgts;
    primitives::Emitter ctx;
    for (auto &[pkg, tgts] : m)
    {
        ctx.addLine(pkg.toString());
    }
    return ctx.getText();
}

String list_programs(sw::SwContext &swctx)
{
    auto &m = swctx.getPredefinedTargets();

    primitives::Emitter ctx("  ");
    ctx.addLine("List of detected programs:");

    auto print_program = [&m, &ctx](const sw::PackagePath &p, const String &title)
    {
        ctx.increaseIndent();
        auto i = m.find(p);
        if (i != m.end(p) && !i->second.empty())
        {
            ctx.addLine(title + ":");
            ctx.increaseIndent();
            if (!i->second.releases().empty())
                ctx.addLine("release:");

            auto add_archs = [](auto &tgts)
            {
                String a;
                for (auto &tgt : tgts)
                {
                    auto &s = tgt->getSettings();
                    if (s["os"]["arch"])
                        a += s["os"]["arch"].getValue() + ", ";
                }
                if (!a.empty())
                {
                    a.resize(a.size() - 2);
                    a = " (" + a + ")";
                }
                return a;
            };

            ctx.increaseIndent();
            for (auto &[v,tgts] : i->second.releases())
            {
                ctx.addLine("- " + v.toString());
                ctx.addText(add_archs(tgts));
            }
            ctx.decreaseIndent();
            if (std::any_of(i->second.begin(), i->second.end(), [](const auto &p) { return !p.first.isRelease(); }))
            {
                ctx.addLine("preview:");
                ctx.increaseIndent();
                for (auto &[v, tgts] : i->second)
                {
                    if (v.isRelease())
                        continue;
                    ctx.addLine("- " + v.toString());
                    ctx.addText(add_archs(tgts));
                }
                ctx.decreaseIndent();
            }
            ctx.decreaseIndent();
        }
        ctx.decreaseIndent();
    };

    print_program("com.Microsoft.VisualStudio.VC.cl", "Microsoft Visual Studio C/C++ Compiler (short form - msvc)");
    print_program("org.LLVM.clang", "Clang C/C++ Compiler (short form - clang)");
    print_program("org.LLVM.clangcl", "Clang C/C++ Compiler in MSVC compatibility mode (short form - clangcl)");

    ctx.addLine();
    ctx.addLine("Use short program form plus version to select it for use.");
    ctx.addLine("   short-version");
    ctx.addLine("Examples: msvc-19.16, msvc-19.24-preview, clang-10");

    return ctx.getText();
}

Programs list_compilers(sw::SwContext &swctx)
{
    auto &m = swctx.getPredefinedTargets();

    Programs progs;

    auto print_program = [&m, &swctx, &progs](const sw::PackagePath &p, const String &title)
    {
        Program prog;
        prog.name = title;
        auto i = m.find(p);
        if (i != m.end(p) && !i->second.empty())
        {
            for (auto &[v,tgts] : i->second.releases())
                prog.releases[{p, v}] = { &tgts };
            if (std::any_of(i->second.begin(), i->second.end(), [](const auto &p) { return !p.first.isRelease(); }))
            {
                for (auto &[v, tgts] : i->second)
                {
                    if (v.isRelease())
                        continue;
                    prog.prereleases[{p, v}] = { &tgts };
                }
            }
            progs.push_back(prog);
        }
    };

    print_program("com.Microsoft.VisualStudio.VC.cl", "Microsoft Visual Studio C/C++ Compiler");
    print_program("org.LLVM.clang", "Clang C/C++ Compiler");
    print_program("org.LLVM.clangcl", "Clang C/C++ Compiler in MSVC compatibility mode (clang-cl)");

    return progs;
}
