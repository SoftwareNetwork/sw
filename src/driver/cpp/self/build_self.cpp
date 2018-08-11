// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef SW_PACKAGE_API
#define SW_PACKAGE_API
#endif

#include <sw/driver/cpp/sw.h>

#include <primitives/context.h>

#include <boost/algorithm/string.hpp>
#include <directories.h>

UnresolvedPackages pkgs;

struct pkg_map
{
    pkg_map()
    {
        std::ifstream ifile(getDirectories().storage_dir_etc / "self.txt");
        if (!ifile)
            return;
        while (1)
        {
            String k, v;
            ifile >> k;
            if (!ifile)
                break;
            ifile >> v;
            m[k] = v;
        }
    }

    ~pkg_map()
    {
        std::ofstream ofile(getDirectories().storage_dir_etc / "self.txt");
        for (auto &[k, v] : m)
            ofile << k << " " << v << "\n";
    }

    std::map<String, String> m;
};

// returns real version
std::tuple<path, Version> getDirSrc(const String &p)
{
    static pkg_map m;

    auto i = m.m.find(p);
    if (i != m.m.end())
    {
        PackageId real_pkg(i->second);
        auto d = real_pkg.getDirSrc();
        if (fs::exists(d))
            return { d, real_pkg.getVersion() };
    }

    auto pkg = extractFromString(p);
    auto real_pkg = resolve_dependencies({ pkg })[pkg];

    auto d = real_pkg.getDirSrc();
    if (!fs::exists(d))
        throw std::runtime_error("Cannot resolve dep: " + p);
    m.m[p] = real_pkg.toString();
    return { d, real_pkg.getVersion() };
}

static void resolve()
{
    resolveAllDependencies(pkgs);
}

#include <build_self.generated.h>

template <class T>
auto &addTarget(Solution &s, const PackagePath &p, const String &v)
{
    auto &t = s.TargetBase::addTarget<T>(p, v);
    auto[d, v2] = getDirSrc(p.toString() + "-" + v);
    t.SourceDir = d;
    t.pkg.version = v2;
    t.pkg.createNames();
    t.init();
    return t;
}

void build_other(Solution &s)
{
    build_self_generated(s);

    auto emb = "pub.egorpugin.primitives.tools.embedder-master"_dep;

    const path cppan2_base = path(__FILE__).parent_path().parent_path().parent_path().parent_path().parent_path();

    {
        auto &support = s.addTarget<LibraryTarget>("support");
        support.CPPVersion = CPPLanguageStandard::CPP17;
        support.Public +=
            "pub.egorpugin.primitives.http-master"_dep,
            "pub.egorpugin.primitives.hash-master"_dep,
            "pub.egorpugin.primitives.command-master"_dep,
            "pub.egorpugin.primitives.log-master"_dep,
            "pub.egorpugin.primitives.executor-master"_dep,
            "org.sw.demo.boost.property_tree-1"_dep,
            "org.sw.demo.boost.stacktrace-1"_dep,
            "org.sw.demo.boost.dll-1"_dep;
        support.SourceDir = cppan2_base / "src/support";
        support += ".*"_rr;
        support.ApiName = "SW_SUPPORT_API";
        if (s.Settings.TargetOS.Type == OSType::Windows)
            support.Public += "UNICODE"_d;

        auto &protos = s.addTarget<StaticLibraryTarget>("protos");
        protos.CPPVersion = CPPLanguageStandard::CPP17;
        protos.SourceDir = cppan2_base / "src" / "protocol";
        protos += ".*"_rr;
        protos.Public +=
            "org.sw.demo.google.grpc.grpcpp-1"_dep,
            "pub.egorpugin.primitives.log-master"_dep;
        gen_grpc(protos, protos.SourceDir / "api.proto", true);

        auto &manager = s.addTarget<LibraryTarget>("manager");
        manager.ApiName = "SW_MANAGER_API";
        //manager.ExportIfStatic = true;
        manager.CPPVersion = CPPLanguageStandard::CPP17;
        manager.Public += support, protos,
            "pub.egorpugin.primitives.yaml-master"_dep,
            "pub.egorpugin.primitives.date_time-master"_dep,
            "pub.egorpugin.primitives.lock-master"_dep,
            "pub.egorpugin.primitives.pack-master"_dep,
            "org.sw.demo.nlohmann.json-3"_dep,
            "org.sw.demo.boost.variant-1"_dep,
            "org.sw.demo.boost.dll-1"_dep,
            "pub.egorpugin.primitives.db.sqlite3-master"_dep,
            "org.sw.demo.rbock.sqlpp11_connector_sqlite3-0"_dep,
            "pub.egorpugin.primitives.version-master"_dep,
            "pub.egorpugin.primitives.win32helpers-master"_dep;
        manager.SourceDir = cppan2_base;
        manager += "src/manager/.*"_rr, "include/manager/.*"_rr;
        manager.Public += "include"_idir, "src/manager"_idir;
        manager.Public += "VERSION_MAJOR=0"_d;
        manager.Public += "VERSION_MINOR=3"_d;
        manager.Public += "VERSION_PATCH=0"_d;
        {
            {
                auto d = manager + emb;
                d->Dummy = true;
            }

            auto c = std::make_shared<Command>();
            c->setProgram(emb);
            c->working_directory = manager.SourceDir / "src/manager/inserts";
            c->args.push_back((manager.SourceDir / "src/manager/inserts/inserts.cpp.in").u8string());
            c->args.push_back((manager.BinaryDir / "inserts.cpp").u8string());
            c->addInput(manager.SourceDir / "src/builder/manager/inserts.cpp.in");
            c->addOutput(manager.BinaryDir / "inserts.cpp");
            manager += manager.BinaryDir / "inserts.cpp";
        }
        gen_sqlite2cpp(manager, manager.SourceDir / "src/manager/inserts/packages_db_schema.sql", "db_packages.h", "db::packages");
        gen_sqlite2cpp(manager, manager.SourceDir / "src/manager/inserts/service_db_schema.sql", "db_service.h", "db::service");

        auto &builder = s.addTarget<LibraryTarget>("builder");
        builder.ApiName = "SW_BUILDER_API";
        //builder.ExportIfStatic = true;
        builder.CPPVersion = CPPLanguageStandard::CPP17;
        builder.Public += manager, "org.sw.demo.preshing.junction-master"_dep;
        builder.SourceDir = cppan2_base;
        builder += "src/builder/.*"_rr, "include/builder/.*"_rr;
        builder.Public += "include"_idir, "src/builder"_idir;
        builder -= "src/builder/db_sqlite.*"_rr;

        auto &cpp_driver = s.addTarget<LibraryTarget>("driver.cpp");
        cpp_driver.ApiName = "SW_DRIVER_CPP_API";
        //cpp_driver.ExportIfStatic = true;
        cpp_driver.CPPVersion = CPPLanguageStandard::CPP17;
        cpp_driver.Public += builder,
            "org.sw.demo.boost.assign-1"_dep,
            "org.sw.demo.boost.uuid-1"_dep,
            "pub.egorpugin.primitives.context-master"_dep;
        cpp_driver.SourceDir = cppan2_base;
        cpp_driver += "src/driver/cpp/.*"_rr, "include/driver/cpp/.*"_rr;
        cpp_driver.Public += "include"_idir, "src/driver/cpp"_idir;
        {
            {
                auto d = cpp_driver + emb;
                d->Dummy = true;
            }

            auto c = std::make_shared<Command>();
            c->setProgram(emb);
            c->working_directory = cpp_driver.SourceDir / "src/driver/cpp/inserts";
            c->args.push_back((cpp_driver.SourceDir / "src/driver/cpp/inserts/inserts.cpp.in").u8string());
            c->args.push_back((cpp_driver.BinaryDir / "inserts.cpp").u8string());
            c->addInput(cpp_driver.SourceDir / "src/driver/cpp/inserts/inserts.cpp.in");
            c->addOutput(cpp_driver.BinaryDir / "inserts.cpp");
            cpp_driver += cpp_driver.BinaryDir / "inserts.cpp";
        }



        /*{
        auto in = builder.SourceDir.parent_path() / "inserts/inserts.cpp.in";
        auto out = builder.BinaryDir / "inserts.cpp";
        auto c = std::make_shared<Command>();
        c->program = inserter.getOutputFile();
        c->args.push_back(in.string());
        c->args.push_back(out.string());
        c->addInput(in);
        c->addOutput(out);
        c->working_directory = builder.SourceDir.parent_path();
        builder += out;
        }

    auto &taywee_args = addTarget<LibraryTarget>(s, "pvt.cppan.demo.taywee.args", "6");
    {
        taywee_args += "args.hxx";
    }

        auto &client = s.addTarget<ExecutableTarget>("client");
        client.CPPVersion = CPPLanguageStandard::CPP17;
        client += builder, taywee_args;
        client.SourceDir = cppan2_base / "src/client";
        client += ".*"_rr;*/

        //s.TargetsToBuild.add(client);
    }
}

void check_self(Checker &c)
{
    check_self_generated(c);
}

void build_self(Solution &s)
{
    s.Settings.Native.LibrariesType = LibraryType::Static;

    SwapAndRestore sr(s.Local, false);
    build_other(s);
}
