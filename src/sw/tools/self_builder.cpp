// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <sw/manager/database.h>
#include <sw/manager/settings.h>
#include <sw/manager/storage.h>
#include <sw/core/sw_context.h>
#include <sw/core/input_database.h>

#include <primitives/emitter.h>
#include <primitives/executor.h>
#include <primitives/sw/main.h>
#include <primitives/sw/cl.h>
#include <primitives/sw/settings_program_name.h>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "self_builder");

#define SW_DRIVER_NAME "org.sw.sw.client.driver.cpp-" PACKAGE_VERSION

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
    write_file(packages, ctx_packages.getText());
}

void write_build_script(
    const std::unordered_map<UnresolvedPackage, LocalPackage> &m,
    const std::unordered_map<UnresolvedPackage, size_t> &gns,
    const std::unordered_map<LocalPackage, size_t> &gns2
    )
{
    auto get_gn = [&gns](auto &u)
    {
        auto i = gns.find(u);
        SW_ASSERT(i != gns.end(), "not found: " + u.toString());
        return i->second;
    };
    auto get_gn2 = [&gns2](auto &u)
    {
        auto i = gns2.find(u);
        SW_ASSERT(i != gns2.end(), "not found 2: " + u.toString());
        return i->second;
    };

    std::map<size_t, std::set<LocalPackage>> used_gns;
    std::vector<LocalPackage> lpkgs;

    // some packages must be before others
    std::vector<UnresolvedPackage> prepkgs;

    // goes before primitives
    prepkgs.push_back("org.sw.demo.ragel-6"s); // keep upkg same as in deps!!!

//#ifdef _WIN32
    // goes before primitives
    prepkgs.push_back("org.sw.demo.lexxmark.winflexbison.bison"s);
//#endif

    // goes before grpc
    prepkgs.push_back("org.sw.demo.google.protobuf.protobuf"s);

    // goes before sw cpp driver (client)
    prepkgs.push_back("org.sw.demo.google.grpc.cpp.plugin"s);

    // goes before sw cpp driver (client)
    prepkgs.push_back("pub.egorpugin.primitives.filesystem-master"s);

    // cpp driver
    prepkgs.push_back({SW_DRIVER_NAME});

    for (auto &u : prepkgs)
    {
        const LocalPackage *lp = nullptr;
        for (auto &[u2, lp2] : m)
        {
            if (u2.ppath == u.ppath)
                lp = &lp2;
        }
        if (!lp)
            throw SW_RUNTIME_ERROR("Cannot find dependency: " + u.toString());

        auto &r = *lp;
        if (used_gns.find(get_gn(u)) != used_gns.end())
        {
            used_gns[get_gn(u)].insert(*lp);
            continue;
        }
        used_gns[get_gn(u)].insert(*lp);
        lpkgs.emplace_back(r);
    }

    for (auto &[u, r] : m)
    {
        if (used_gns.find(get_gn(u)) != used_gns.end())
        {
            used_gns[get_gn(u)].insert(r);
            continue;
        }
        used_gns[get_gn(u)].insert(r);
        lpkgs.emplace_back(r);
    }

    primitives::CppEmitter build;
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
    }

    //
    build.beginFunction("TargetEntryPointMap load_builtin_entry_points()");
    build.addLine("TargetEntryPointMap epm;");
    build.addLine();
    for (auto &r : lpkgs)
    {
        auto f = read_file(r.getDirSrc2() / "sw.cpp");
        bool has_checks = f.find("Checker") != f.npos; // more presize than setChecks

        // enumerate all other packages in group
        for (auto &p : used_gns[get_gn2(r)])
        {
            build.beginBlock();
            build.addLine("auto ep = std::make_unique<sw::NativeBuiltinTargetEntryPoint>(build_" + r.getVariableName() + ");");
            if (has_checks)
                build.addLine("ep->cf = check_" + r.getVariableName() + ";");
            build.addLine("epm.emplace(\"" + p.toString() + "\"s, std::move(ep));");
            build.endBlock();
        }
        build.addLine();
    }
    build.addLine("return epm;");
    build.endFunction();

    ctx += build;

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

    SwCoreContext swctx(Settings::get_user_settings().storage_dir);
    auto m = swctx.install(
    {
        // our main cpp driver target
        {SW_DRIVER_NAME},

        // other needed stuff (libcxx)
        {"org.sw.demo.llvm_project.libcxx"},
    });

    // calc GNs
    std::unordered_map<UnresolvedPackage, size_t> gns;
    std::unordered_map<LocalPackage, size_t> gns2;
    auto &idb = swctx.getInputDatabase();
    for (auto &[u, r] : m)
    {
        gns[u] = idb.getFileHash(r.getDirSrc2() / "sw.cpp");
        gns2[r] = idb.getFileHash(r.getDirSrc2() / "sw.cpp");
    }

    write_required_packages(m);
    write_build_script(m, gns, gns2);

    return 0;
}

EXPORT_FROM_EXECUTABLE
std::string getProgramName()
{
    return PACKAGE_NAME_CLEAN;
}
