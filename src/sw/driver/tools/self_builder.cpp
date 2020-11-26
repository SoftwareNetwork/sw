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

#define SW_DRIVER_NAME "org.sw.sw.client.driver.cpp-" PACKAGE_VERSION

using namespace sw;

std::unordered_map<size_t, String> hdr_vars;

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
    // our main cpp driver target
    rrs.emplace_back(String(SW_DRIVER_NAME));
    return rrs;
}

String write_build_script_headers(SwCoreContext &swctx)
{
    auto &idb = swctx.getInputDatabase();

    // some packages must be before others
    std::vector<ResolveRequest> prepkgs;
    {
        // keep block order

        {
            // goes before primitives
            prepkgs.emplace_back("org.sw.demo.ragel-6"s); // keep upkg same as in deps!!!

                                                          // goes before primitives
            prepkgs.emplace_back("org.sw.demo.lexxmark.winflexbison.bison"s);

            // goes before grpc
            prepkgs.emplace_back("org.sw.demo.google.protobuf.protobuf"s);

            // goes before sw cpp driver (client)
            prepkgs.emplace_back("org.sw.demo.google.grpc.cpp.plugin"s);

            // goes before sw cpp driver (client)
            prepkgs.emplace_back("pub.egorpugin.primitives.filesystem"s);
        }

        // for gui
        //prepkgs.emplace_back("org.sw.demo.qtproject.qt.base.tools.moc"s);

        {
            // cpp driver
            prepkgs.emplace_back(String(SW_DRIVER_NAME));
        }

        resolveWithDependencies(prepkgs, [&swctx](auto &rr) { return swctx.resolve(rr, true); });
    }

    primitives::CppEmitter ctx;
    std::unordered_set<size_t> hashes;
    for (auto &rr : prepkgs)
    {
        if (!swctx.getLocalStorage().isPackageInstalled(rr.getPackage()) &&
            !swctx.getLocalStorage().isPackageOverridden(rr.getPackage()))
            continue;

        LocalPackage localpkg(swctx.getLocalStorage(), rr.getPackage());

        SpecificationFiles sf;
        sf.addFile("sw.cpp", localpkg.getDirSrc2() / "sw.cpp");
        Specification s(sf);
        auto h = s.getHash(idb);
        if (!hashes.insert(h).second)
            continue;

        auto fn = s.files.getData().begin()->second.absolute_path;
        auto f = read_file(fn);
        bool has_checks = f.find("Checker") != f.npos; // more presize than setChecks

        auto var = localpkg.getVariableName();
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

String write_build_script(SwCoreContext &swctx, const std::vector<ResolveRequest> &m_in)
{
    auto &idb = swctx.getInputDatabase();

    std::set<PackageId> pkgs;
    std::unordered_map<size_t, std::set<PackageId>> hash_pkgs;
    for (auto &rr : m_in)
    {
        auto &lp = dynamic_cast<LocalPackage &>(rr.getPackage());
        SpecificationFiles sf;
        sf.addFile("sw.cpp", lp.getDirSrc2() / "sw.cpp");
        Specification s(sf);
        auto h = s.getHash(idb);
        hash_pkgs[h].insert(lp);
        pkgs.insert(lp);
    }

    primitives::CppEmitter ctx;
    ctx.beginNamespace("sw");
    // eps
    ctx.beginFunction("BuiltinEntryPoints load_builtin_entry_points()");
    ctx.addLine("BuiltinEntryPoints epm;");
    ctx.addLine();
    for (auto &rr : m_in)
    {
        auto &lp = dynamic_cast<LocalPackage&>(rr.getPackage());
        SpecificationFiles sf;
        sf.addFile("sw.cpp", lp.getDirSrc2() / "sw.cpp");
        Specification s(sf);
        auto h = s.getHash(idb);
        if (!hash_pkgs.contains(h))
            continue;
        hash_pkgs.erase(h);

        auto fn = s.files.getData().begin()->second.absolute_path;
        auto f = read_file(fn);
        bool has_checks = f.find("Checker") != f.npos; // more presize than setChecks

        auto var = hdr_vars[h];
        ctx.beginBlock();
        ctx.addLine("auto ep = std::make_unique<NativeBuiltinTargetEntryPoint>(build_" + var + ");");
        if (has_checks)
            ctx.addLine("ep->cf = check_" + var + ";");
        ctx.addLine("epm.emplace_back(" + std::to_string(h) + ", std::move(ep));");
        ctx.endBlock();
        ctx.emptyLines();
    }
    ctx.addLine("return epm;");
    ctx.endFunction();

    // pkgs
    ctx.beginFunction("PackageIdSet load_builtin_packages()");
    ctx.addLine("return");
    ctx.beginBlock();
    for (auto &p : pkgs)
        ctx.addLine("\"" + p.toString() + "\"s,");
    ctx.endBlock(true);
    ctx.endFunction();
    ctx.endNamespace();

    return ctx.getText();
}

int main(int argc, char **argv)
{
    static cl::opt<String> loglevel("log-level", cl::init("INFO"));
    static cl::opt<path> p(cl::Positional, cl::Required);
    static cl::opt<path> _(cl::Positional, cl::ConsumeAfter); // to be removed

    cl::ParseCommandLineOptions(argc, argv);

    // init
    setup_log(loglevel);
    primitives::http::setupSafeTls();

    Executor e(select_number_of_threads());
    getExecutor(&e);

    //
    SwCoreContext swctx(Settings::get_user_settings().storage_dir, true);
    auto m_rrs = get_base_rr_vector();
    resolveWithDependencies(m_rrs, [&swctx](auto &rr) { return swctx.resolve(rr, true); });
    {
        // mass (threaded) install!
        auto &e = getExecutor();
        Futures<void> fs;
        for (auto &rr : m_rrs)
        {
            fs.push_back(e.push([&swctx, &rr]
            {
                swctx.install(rr);
            }));
        }
        waitAndGet(fs);
    }

    // we do second resolve, because we need this packages to be included before driver's sw.cpp,
    // but we do not need to install them on user system
    //
    //auto m_headers_rrs = get_base_rr_vector();
    //m_headers_rrs.emplace_back("org.sw.demo.llvm_project.libcxx"s); // other needed stuff (libcxx)
    //m_headers_rrs.emplace_back("org.sw.demo.qtproject.qt.base.tools.moc"s); // for gui
    //resolveWithDependencies(m_headers_rrs, [&swctx](auto &rr) { return swctx.resolve(rr, true); });

    auto t2 = write_build_script_headers(swctx);
    auto t3 = write_build_script(swctx, m_rrs);
    write_file(p, t2 + t3);

    return 0;
}

EXPORT_FROM_EXECUTABLE
std::string getProgramName()
{
    return PACKAGE_NAME_CLEAN;
}
