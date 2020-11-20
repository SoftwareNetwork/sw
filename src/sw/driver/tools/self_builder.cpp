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

String write_required_packages(const std::vector<ResolveRequest> &rrs)
{
    StringSet upkgs_sorted;
    for (auto &rr : rrs)
        upkgs_sorted.insert(rr.u.toString());

    primitives::CppEmitter ctx_packages;
    for (auto &s : upkgs_sorted)
        ctx_packages.addLine("\"" + s + "\"s,");
    return ctx_packages.getText();
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
        prepkgs.emplace_back("org.sw.demo.qtproject.qt.base.tools.moc"s);

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

        ctx.addLine("#define configure configure_" + localpkg.getVariableName());
        ctx.addLine("#define build build_" + localpkg.getVariableName());
        if (has_checks)
            ctx.addLine("#define check check_" + localpkg.getVariableName());
        ctx.addLine("#include \"" + to_string(normalize_path(fn)) + "\"");
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

    // create specs
    /*std::unordered_map<UnresolvedPackage, Specification> gns;
    std::unordered_map<LocalPackage, Specification> gns2;
    for (auto &[u, r] : m)
    {
        SpecificationFiles f;
        f.addFile("sw.cpp", r.getDirSrc2() / "sw.cpp");
        Specification s(f);
        gns2.emplace(r, s);
        gns.emplace(u, s);
    }

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
    };*/

    //std::map<size_t, std::set<LocalPackage>> used_gns;
    //std::vector<std::pair<LocalPackage, Specification>> lpkgs;

    /*for (auto &u : prepkgs)
    {
        const LocalPackage *lp = nullptr;
        for (auto &[u2, lp2] : m)
        {
            if (u2.ppath == u.ppath)
                lp = &lp2;
        }
        if (!lp)
            throw SW_RUNTIME_ERROR("Cannot find dependency: " + u.toString());

        const auto &s = get_gn(u);
        auto h = s.getHash(idb);
        auto &r = *lp;
        if (used_gns.find(h) != used_gns.end())
        {
            used_gns[h].insert(*lp);
            continue;
        }
        used_gns[h].insert(*lp);
        lpkgs.emplace_back(r, s);
    }

    for (auto &[u, r] : m)
    {
        const auto &s = get_gn(u);
        auto h = s.getHash(idb);
        if (used_gns.find(h) != used_gns.end())
        {
            used_gns[h].insert(r);
            continue;
        }
        used_gns[h].insert(r);
        lpkgs.emplace_back(r, s);
    }*/

    std::unordered_map<size_t, std::set<PackageId>> hash_pkgs;
    for (auto &rr : m_in)
    {
        auto &lp = dynamic_cast<LocalPackage &>(rr.getPackage());
        SpecificationFiles sf;
        sf.addFile("sw.cpp", lp.getDirSrc2() / "sw.cpp");
        Specification s(sf);
        auto h = s.getHash(idb);
        hash_pkgs[h].insert(lp);
    }

    primitives::CppEmitter ctx;
    // function
    ctx.beginNamespace("sw");
    ctx.beginFunction("BuiltinInputs load_builtin_inputs(SwContext &swctx, const IDriver &d)");
    ctx.addLine("BuiltinInputs epm;");
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

        auto fn = s.files.getData().begin()->second.absolute_path;
        auto f = read_file(fn);
        bool has_checks = f.find("Checker") != f.npos; // more presize than setChecks

        ctx.beginBlock();
        ctx.addLine("auto i = std::make_unique<BuiltinInput>(swctx, d, " + std::to_string(h) + ");");
        ctx.addLine("auto ep = std::make_unique<sw::NativeBuiltinTargetEntryPoint>(build_" + lp.getVariableName() + ");");
        if (has_checks)
            ctx.addLine("ep->cf = check_" + lp.getVariableName() + ";");
        ctx.addLine("i->setEntryPoint(std::move(ep));");
        ctx.addLine("auto [ii, _] = swctx.registerInput(std::move(i));");

        // enumerate all other packages in group
        for (auto &p : hash_pkgs[h])
            ctx.addLine("epm[ii].insert(\"" + p.toString() + "\"s);");
        hash_pkgs.erase(h);
        ctx.endBlock();
        ctx.emptyLines();
    }
    ctx.addLine("return epm;");
    ctx.endFunction();
    ctx.endNamespace();

    return ctx.getText();
}

int main(int argc, char **argv)
{
    static cl::opt<String> loglevel("log-level", cl::init("INFO"));
    static cl::opt<path> p(cl::Positional, cl::Required);
    static cl::opt<path> packages(cl::Positional, cl::Required);

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
    auto t1 = write_required_packages(m_rrs);
    write_file(packages, t1);

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
