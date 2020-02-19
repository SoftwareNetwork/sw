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

#include <cl.llvm.h>

void setHttpSettings(const Options &options)
{
    httpSettings.verbose = options.curl_verbose;
    httpSettings.ignore_ssl_checks = options.ignore_ssl_checks;
    httpSettings.proxy = sw::Settings::get_local_settings().proxy;
}

std::unique_ptr<sw::SwContext> createSwContext(const Options &options)
{
    // load proxy settings before ctx
    setHttpSettings(options);

    auto swctx = std::make_unique<sw::SwContext>(options.storage_dir.empty() ? sw::Settings::get_user_settings().storage_dir : options.storage_dir);
    // TODO:
    // before default?
    //for (auto &d : drivers)
    //swctx->registerDriver(std::make_unique<sw::driver::cpp::Driver>());
    swctx->registerDriver("org.sw.sw.driver.cpp-0.4.1"s, std::make_unique<sw::driver::cpp::Driver>());
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
