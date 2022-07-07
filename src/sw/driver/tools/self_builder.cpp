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
#define QT_VERSION ""

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

String write_required_packages(const std::unordered_map<UnresolvedPackage, LocalPackage> &m)
{
    StringSet pkgs_sorted;
    for (auto &[p, d] : m)
        pkgs_sorted.insert(d.toString());

    primitives::CppEmitter ctx_packages;
    for (auto &s : pkgs_sorted)
        ctx_packages.addLine("\"" + s + "\"s,");
    return ctx_packages.getText();
}

String write_build_script(SwCoreContext &swctx,
    const std::unordered_map<UnresolvedPackage, LocalPackage> &m_in,
    bool headers
)
{
    const std::map<UnresolvedPackage, LocalPackage> m(m_in.begin(), m_in.end()); // keep order
    auto &idb = swctx.getInputDatabase();

    // create specs
    std::unordered_map<UnresolvedPackage, Specification> gns;
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
        SW_ASSERT(i != gns.end(), "not found: " + u.toString() + ": do 'sw override org.sw' in sw client dir and check that this package is added to some storage");
        return i->second;
    };
    auto get_gn2 = [&gns2](auto &u)
    {
        auto i = gns2.find(u);
        SW_ASSERT(i != gns2.end(), "not found 2: " + u.toString());
        return i->second;
    };

    std::map<size_t, std::set<LocalPackage>> used_gns;
    std::vector<std::pair<LocalPackage, Specification>> lpkgs;

    // some packages must be before others
    std::vector<UnresolvedPackage> prepkgs;
    {
        // keep block order

        {
            // goes before primitives
            prepkgs.push_back("org.sw.demo.ragel-6"s); // keep upkg same as in deps!!!

            // goes before primitives
            prepkgs.push_back("org.sw.demo.lexxmark.winflexbison.bison"s);

            // goes before grpc
            prepkgs.push_back("org.sw.demo.google.protobuf.protobuf"s);

            // goes before sw cpp driver (client)
            prepkgs.push_back("org.sw.demo.google.grpc.cpp.plugin"s);

            // goes before sw cpp driver (client)
            prepkgs.push_back("pub.egorpugin.primitives.filesystem" PRIMITIVES_VERSION ""s);
        }

        if (headers)
        {
            // for gui
            prepkgs.push_back("org.sw.demo.qtproject.qt.base.tools.moc" QT_VERSION ""s);
        }

        {
            // cpp driver
            prepkgs.push_back({ SW_DRIVER_NAME });
        }
    }

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
    }

    // includes
    primitives::CppEmitter ctx;
    if (headers)
    {
        for (auto &[r, s] : lpkgs)
        {
            auto fn = s.files.getData().begin()->second.absolute_path;
            auto f = read_file(fn);
            bool has_checks = f.find("Checker") != f.npos; // more presize than setChecks

            auto &d = r.getData();
            ctx.addLine("#define configure configure_" + r.getVariableName());
            ctx.addLine("#define build build_" + r.getVariableName());
            if (has_checks)
                ctx.addLine("#define check check_" + r.getVariableName());
            ctx.addLine("#include \"" + to_string(normalize_path(fn)) + "\"");
            ctx.addLine("#undef configure");
            ctx.addLine("#undef build");
            if (has_checks)
                ctx.addLine("#undef check");
            ctx.addLine();
        }
    }

    auto &build = ctx.createInlineEmitter<primitives::CppEmitter>();

    if (headers)
    {
        ctx.addLine("#undef build");
        ctx.addLine("#undef check");
        ctx.addLine("#undef configure");
    }

    if (headers)
        return ctx.getText();

    // function
    build.beginNamespace("sw");
    build.beginFunction("BuiltinInputs load_builtin_inputs(SwContext &swctx, const IDriver &d)");
    build.addLine("BuiltinInputs epm;");
    build.addLine();
    for (auto &[r,s] : lpkgs)
    {
        auto fn = s.files.getData().begin()->second.absolute_path;
        auto f = read_file(fn);
        bool has_checks = f.find("Checker") != f.npos; // more presize than setChecks

        build.beginBlock();
        build.addLine("auto i = std::make_unique<BuiltinInput>(swctx, d, " + std::to_string(s.getHash(idb)) + ");");
        build.addLine("auto ep = std::make_unique<sw::NativeBuiltinTargetEntryPoint>(build_" + r.getVariableName() + ");");
        if (has_checks)
            build.addLine("ep->cf = check_" + r.getVariableName() + ";");
        build.addLine("i->setEntryPoint(std::move(ep));");
        build.addLine("auto [ii, _] = swctx.registerInput(std::move(i));");

        // enumerate all other packages in group
        for (auto &p : used_gns[get_gn2(r).getHash(idb)])
            build.addLine("epm[ii].insert(\"" + p.toString() + "\"s);");
        build.endBlock();
        build.emptyLines();
    }
    build.addLine("return epm;");
    build.endFunction();
    build.endNamespace();

    return ctx.getText();
}

int main(int argc, char *argv[])
{
    static cl::opt<String> loglevel("log-level", cl::init("INFO"));
    static cl::opt<path> p(cl::Positional, cl::Required);
    static cl::opt<path> packages(cl::Positional, cl::Required);

    cl::ParseCommandLineOptions(argc, argv);

    // init
    setup_log(loglevel);
    primitives::http::setupSafeTls();

    //
    SwCoreContext swctx(Settings::get_user_settings().storage_dir, true);
    swctx.executor = std::make_unique<Executor>(select_number_of_threads());
    auto m = swctx.install(
    {
        // our main cpp driver target
        {SW_DRIVER_NAME},
    });
    auto t1 = write_required_packages(m);
    write_file(packages, t1);

    // we do second install, because we need this packages to be included before driver's sw.cpp,
    // but we do not need to install them on user system
    auto m_headers = swctx.install(
    {
        // our main cpp driver target
        {SW_DRIVER_NAME},

        // other needed stuff (libcxx)
        {"org.sw.demo.llvm_project.libcxx"},

        // for gui
        {"org.sw.demo.qtproject.qt.base.tools.moc" QT_VERSION},
    });
    auto t2 = write_build_script(swctx, m_headers, true);
    auto t3 = write_build_script(swctx, m, false);
    write_file(p, t2 + t3);

    return 0;
}
