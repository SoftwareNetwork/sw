// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#include <sw/manager/database.h>
#include <sw/manager/settings.h>
#include <sw/manager/storage.h>
#include <sw/core/sw_context.h>
#include <sw/core/input_database.h>
#include <sw/core/specification.h>

#include <primitives/emitter.h>
#include <primitives/executor.h>
#include <primitives/http.h>
#include <primitives/sw/main.h>
#include <primitives/sw/cl.h>
#include <primitives/sw/settings_program_name.h>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "self_builder");

#define SW_DRIVER_NAME "org.sw.sw.client.driver.cpp-" PACKAGE_VERSION ""

using namespace sw;

std::unordered_map<size_t, String> hdr_vars;
PackageSettings empty_settings;

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

auto get_base_rr_vector()
{
    std::vector<ResolveRequest> rrs;
#define DEP1(x) x
#define DEP(x) rrs.emplace_back(ResolveRequest{ DEP1(String{x}), empty_settings });
#include "self_builder.inl"
#undef DEP
#undef DEP1

    return rrs;
}

String getVariableName(const PackageName &n)
{
    auto vname = n.getPath().toString() + "_" + n.getVersion().toString();
    std::replace(vname.begin(), vname.end(), '.', '_');
    return vname;
}

String write_build_script_headers(SwCoreContext &swctx, const std::vector<ResolveRequest> &rrs)
{
    auto &idb = swctx.getInputDatabase();

    primitives::CppEmitter ctx;
    std::unordered_set<size_t> hashes;
    for (auto &rr : rrs)
    {
        // this check was intended for qt deps (moc etc.)
        // they should be skipped from adding as includes
        /*if (!swctx.getLocalStorage().isPackageInstalled(rr.getPackage()) &&
            !swctx.getLocalStorage().isPackageOverridden(rr.getPackage()))
            continue;*/

        SpecificationFiles sf;
        sf.addFile("sw.cpp", rr.getPackage().getSourceDirectory() / "sw.cpp");
        Specification s(sf);
        auto h = s.getHash(idb);
        if (!hashes.insert(h).second)
            continue;

        auto fn = s.files.getData().begin()->second.absolute_path;
        auto f = read_file(fn);
        bool has_checks = f.find("Checker") != f.npos; // more presize than setChecks

        auto var = getVariableName(rr.getPackage().getId().getName());
        hdr_vars[h] = var;

        ctx.addLine("#define configure configure_" + var);
        ctx.addLine("#define build build_" + var);
        if (has_checks)
            ctx.addLine("#define check check_" + var);
        //ctx.beginNamespace(var);
        ctx.addLine("#include \"" + to_string(normalize_path(fn)) + "\"");
        //ctx.addLine();
        //ctx.endNamespace();
        ctx.addLine("#undef configure");
        ctx.addLine("#undef build");
        if (has_checks)
            ctx.addLine("#undef check");
        ctx.addLine();
    }
    return ctx.getText();
}

String write_build_script(SwCoreContext &swctx, const std::vector<ResolveRequest> &rrs)
{
    auto &idb = swctx.getInputDatabase();

    std::unordered_map<size_t, std::unordered_map<UnresolvedPackageName, PackageName>> hash_pkgs;
    for (auto &rr : rrs)
    {
        SpecificationFiles sf;
        sf.addFile("sw.cpp", rr.getPackage().getSourceDirectory() / "sw.cpp");
        Specification s(sf);
        auto h = s.getHash(idb);
        hash_pkgs[h].emplace(rr.getUnresolvedPackageName(), rr.getPackage().getId().getName());
    }

    primitives::CppEmitter ctx;
    ctx.beginNamespace("sw");
    // eps
    ctx.beginFunction("BuiltinEntryPoints load_builtin_entry_points()");
    ctx.addLine("BuiltinEntryPoints epm;");
    ctx.addLine();
    for (auto &rr : rrs)
    {
        SpecificationFiles sf;
        sf.addFile("sw.cpp", rr.getPackage().getSourceDirectory() / "sw.cpp");
        Specification s(sf);
        auto h = s.getHash(idb);
        if (!hash_pkgs.contains(h))
            continue;

        auto fn = s.files.getData().begin()->second.absolute_path;
        auto f = read_file(fn);
        bool has_checks = f.find("Checker") != f.npos; // more presize than setChecks

        auto var = hdr_vars[h];
        ctx.beginBlock();
        ctx.addLine("auto &e = epm.emplace_back();");
        ctx.addLine("e.bfs.bf = build_" + var + ";");
        if (has_checks)
            ctx.addLine("e.bfs.cf = check_" + var + ";");
        for (auto &&[u, n] : hash_pkgs[h])
        {
            ctx.addLine("e.add_pair(\"" + u.toString() + "\"s, \"" + n.toString() + "\"s);");
            // also add direct resolve
            ctx.addLine("e.add_pair(\"" + UnresolvedPackageName{ n }.toString() + "\"s, \"" + n.toString() + "\"s);");
        }
        ctx.endBlock();
        ctx.emptyLines();

        hash_pkgs.erase(h);
    }
    ctx.addLine("return epm;");
    ctx.endFunction();
    //
    ctx.endNamespace();

    return ctx.getText();
}

int main(int argc, char **argv)
{
    static cl::opt<String> loglevel("log-level", cl::init("INFO"));
    static cl::opt<path> p(cl::Positional, cl::Required);

    cl::ParseCommandLineOptions(argc, argv);

    // init
    setup_log(loglevel);
    primitives::http::setupSafeTls();

    Executor e(select_number_of_threads());
    getExecutor(&e);

    //
    SwCoreContext swctx(Settings::get_user_settings().storage_dir, true);
    auto rrs = get_base_rr_vector();
    for (auto &&rr : rrs)
    {
        if (!swctx.resolve(rr, true))
            throw SW_RUNTIME_ERROR("Not resolved: " + rr.getUnresolvedPackageName().toString());
    }
    {
        //for (auto &&rr : rrs)
            //swctx.getLocalStorage().install(rr.getPackage());

        // mass (threaded) install!
        auto &e = getExecutor();
        Futures<void> fs;
        for (auto &rr : rrs)
        {
            fs.push_back(e.push([&swctx, &rr]
            {
                auto lp = swctx.getLocalStorage().install(rr.getPackage());
                if (lp)
                    rr.setPackageForce(std::move(lp));
            }));
        }
        waitAndGet(fs);
    }

    auto t2 = write_build_script_headers(swctx, rrs);
    auto t3 = write_build_script(swctx, rrs);
    write_file(p, t2 + t3);

    return 0;
}

EXPORT_FROM_EXECUTABLE
std::string getProgramName()
{
    return PACKAGE_NAME_CLEAN;
}
