// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <sw/manager/database.h>
#include <sw/manager/settings.h>
#include <sw/manager/storage.h>
#include <sw/manager/sw_context.h>

#include <primitives/emitter.h>
#include <primitives/executor.h>
#include <primitives/sw/main.h>
#include <primitives/sw/cl.h>
#include <primitives/sw/settings_program_name.h>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "self_builder");

using namespace sw;

static cl::opt<path> p(cl::Positional, cl::Required);
static cl::opt<path> packages(cl::Positional, cl::Required);

void setup_log(const std::string &log_level)
{
    LoggerSettings log_settings;
    log_settings.log_level = log_level;
    log_settings.simple_logger = true;
    log_settings.print_trace = true;
    initLogger(log_settings);

    // first trace message
    LOG_TRACE(logger, "----------------------------------------");
    LOG_TRACE(logger, "Starting sw...");
}

void write_required_packages(const std::unordered_map<UnresolvedPackage, LocalPackage> &m)
{
    StringSet pkgs_sorted;
    for (auto &[p, d] : m)
        pkgs_sorted.insert(d.toString());

    primitives::CppEmitter ctx_packages;
    for (auto &s : pkgs_sorted)
        ctx_packages.addLine("\"" + s + "\"s,");
    write_file_if_different(packages, ctx_packages.getText());
}

void write_build_script(const std::unordered_map<UnresolvedPackage, LocalPackage> &m)
{
    primitives::CppEmitter build;
    build.beginFunction("void build_self_generated(Solution &s)");
    build.addLine("auto sdir_old = s.SourceDir;");
    build.addLine();

    primitives::CppEmitter check;
    check.beginFunction("void check_self_generated(Checker &c)");

    std::set<PackageVersionGroupNumber> used_gns;
    std::vector<LocalPackage> lpkgs;

    // some packages must be before others
    std::vector<UnresolvedPackage> prepkgs{
        {"org.sw.demo.ragel"},
        {"org.sw.demo.ragel-6"},

        {"org.sw.demo.lexxmark.winflexbison.bison-master"},

        {"org.sw.demo.google.protobuf.protobuf"},
        {"org.sw.demo.google.protobuf.protobuf-3"},

        {"org.sw.demo.google.grpc.grpc_cpp_plugin"},
        {"org.sw.demo.google.grpc.grpc_cpp_plugin-1"},
    };

    for (auto &u : prepkgs)
    {
        auto i = m.find(u);
        if (i == m.end())
            continue;
        auto &r = i->second;
        auto &d = r.getData();
        if (used_gns.find(d.group_number) != used_gns.end())
            continue;
        used_gns.insert(d.group_number);
        lpkgs.emplace_back(r);
    }

    for (auto &[u, r] : m)
    {
        auto &d = r.getData();
        if (used_gns.find(d.group_number) != used_gns.end())
            continue;
        used_gns.insert(d.group_number);
        lpkgs.emplace_back(r);
    }

    primitives::CppEmitter ctx;
    for (auto &r : lpkgs)
    {
        auto f = read_file(r.getDirSrc2() / "sw.cpp");
        bool has_checks = f.find("Checker") != f.npos; // more presize than setChecks

        auto &d = r.getData();
        ctx.addLine("#define configure configure_" + r.getVariableName());
        ctx.addLine("#define build build_" + r.getVariableName());
        if (has_checks)
            ctx.addLine("#define check check_" + r.getVariableName());
        ctx.addLine("#include \"" + normalize_path(r.getDirSrc2() / "sw.cpp") + "\"");
        ctx.addLine("#undef configure");
        ctx.addLine("#undef build");
        if (has_checks)
            ctx.addLine("#undef check");
        ctx.addLine();

        build.addLine("s.NamePrefix = \"" + r.ppath.slice(0, d.prefix).toString() + "\";");
        build.addLine("s.current_module = \"" + r.toString() + "\";");
        build.addLine("s.current_gn = " + std::to_string(d.group_number) + ";");
        build.addLine("build_" + r.getVariableName() + "(s);");
        build.addLine();

        if (has_checks)
        {
            check.addLine("c.build.current_gn = " + std::to_string(d.group_number) + ";");
            check.addLine("check_" + r.getVariableName() + "(c);");
            check.addLine();
        }
    }

    build.addLine("s.NamePrefix.clear();");
    build.addLine("s.current_module.clear();");
    build.addLine("s.current_gn = 0;");
    build.endFunction();
    check.addLine("c.build.current_gn = 0;");
    check.endFunction();

    ctx += build;
    ctx += check;

    ctx.addLine("#undef build");
    ctx.addLine("#undef check");
    ctx.addLine("#undef configure");

    write_file(p, ctx.getText());
}

int main(int argc, char **argv)
{
    setup_log("INFO");

    cl::ParseCommandLineOptions(argc, argv);

    // init
    Executor e(select_number_of_threads());
    getExecutor(&e);

    SwManagerContext swctx(Settings::get_user_settings().storage_dir);
    auto m = swctx.install({{"org.sw.sw.client.driver.cpp-0.3.1"}});

    write_required_packages(m);
    write_build_script(m);

    return 0;
}

EXPORT_FROM_EXECUTABLE
std::string getProgramName()
{
    return PACKAGE_NAME_CLEAN;
}
