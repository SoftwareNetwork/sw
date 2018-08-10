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

    auto &zlib = s.getTarget<LibraryTarget>("org.sw.demo.madler.zlib");
    auto &sqlite3 = s.getTarget<LibraryTarget>("org.sw.demo.sqlite3");

    auto &boost_algorithm = s.getTarget<LibraryTarget>("org.sw.demo.boost.algorithm");
    auto &boost_filesystem = s.getTarget<LibraryTarget>("org.sw.demo.boost.filesystem");
    auto &boost_thread = s.getTarget<LibraryTarget>("org.sw.demo.boost.thread");
    auto &boost_asio = s.getTarget<LibraryTarget>("org.sw.demo.boost.asio");
    auto &boost_system = s.getTarget<LibraryTarget>("org.sw.demo.boost.system");
    auto &boost_process = s.getTarget<LibraryTarget>("org.sw.demo.boost.process");
    auto &boost_date_time = s.getTarget<LibraryTarget>("org.sw.demo.boost.date_time");
    auto &boost_interprocess = s.getTarget<LibraryTarget>("org.sw.demo.boost.interprocess");
    auto &boost_log = s.getTarget<LibraryTarget>("org.sw.demo.boost.log");
    auto &boost_dll = s.getTarget<LibraryTarget>("org.sw.demo.boost.dll");
    auto &boost_property_tree = s.getTarget<LibraryTarget>("org.sw.demo.boost.property_tree");
    auto &boost_stacktrace = s.getTarget<LibraryTarget>("org.sw.demo.boost.stacktrace");
    auto &boost_variant = s.getTarget<LibraryTarget>("org.sw.demo.boost.variant");
    auto &boost_assign = s.getTarget<LibraryTarget>("org.sw.demo.boost.assign");
    auto &boost_uuid = s.getTarget<LibraryTarget>("org.sw.demo.boost.uuid");

    auto yaml_cpp = "org.sw.demo.jbeder.yaml_cpp-master"_dep;
    auto libarchive = "org.sw.demo.libarchive.libarchive-3"_dep;
    auto crypto = "org.sw.demo.openssl.crypto-1.*.*.*"_dep;
    auto ssl = "org.sw.demo.openssl.ssl-1.*.*.*"_dep;
    auto c_ares = "org.sw.demo.c_ares-1"_dep;
    auto libcurl = "org.sw.demo.badger.curl.libcurl-7"_dep;


    auto &rhash = addTarget<LibraryTarget>(s, "pvt.cppan.demo.aleksey14.rhash", "1");
    {
        rhash.ApiName = "RHASH_API";
        rhash +=
            "librhash/.*\\.c"_rr,
            "librhash/.*\\.h"_rr,
            "win32/.*\\.h"_rr;

        rhash.Public +=
            "."_id,
            "librhash"_id;
    }

    auto &date = addTarget<LibraryTarget>(s, "pvt.cppan.demo.howardhinnant.date.date", "2");
    /*{
    date +=
    "date.h";

    date.Public +=
    "."_id;
    }*/

    auto &sqlpp11 = addTarget<LibraryTarget>(s, "pvt.cppan.demo.rbock.sqlpp11", "0");
    {
        sqlpp11 +=
            "include/.*"_rr;

        sqlpp11.Public +=
            "include"_id;

        sqlpp11.Public += date;
    }

    auto &sqlpp11_connector_sqlite3 = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.rbock.sqlpp11_connector_sqlite3", "0");
    {
        sqlpp11_connector_sqlite3 +=
            "include/.*"_rr,
            "src/.*"_rr;

        sqlpp11_connector_sqlite3.Private +=
            "src"_id;

        sqlpp11_connector_sqlite3.Public +=
            "include"_id;

        sqlpp11_connector_sqlite3.Public += sqlpp11, sqlite3, date;
    }

    auto &turf = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.preshing.turf", "master");
    {
        turf +=
            "cmake/Macros.cmake",
            "cmake/turf_config.h.in",
            "turf/.*"_rr;

        turf.Public +=
            "."_id;

        turf.Variables["TURF_USERCONFIG"];
        turf.Variables["TURF_ENABLE_CPP11"] = "1";
        turf.Variables["TURF_WITH_BOOST"] = "FALSE";
        turf.Variables["TURF_WITH_EXCEPTIONS"] = "FALSE";
        if (s.Settings.Native.CompilerType == CompilerType::MSVC)
            turf.Variables["TURF_WITH_SECURE_COMPILER"] = "FALSE";
        turf.Variables["TURF_REPLACE_OPERATOR_NEW"] = "FALSE";

        turf.Variables["TURF_HAS_LONG_LONG"] = "1";
        turf.Variables["TURF_HAS_STDINT"] = "1";
        turf.Variables["TURF_HAS_NOEXCEPT"] = "1";
        turf.Variables["TURF_HAS_CONSTEXPR"] = "1";
        turf.Variables["TURF_HAS_OVERRIDE"] = "1";
        turf.Variables["TURF_HAS_STATIC_ASSERT"] = "1";
        turf.Variables["TURF_HAS_MOVE"] = "1";

        turf.configureFile("cmake/turf_config.h.in", "turf_config.h");
        turf.fileWriteOnce("turf_userconfig.h", "", true);
    }

    auto &junction = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.preshing.junction", "master");
    {
        junction +=
            "cmake/junction_config.h.in",
            "junction/.*"_rr;

        junction.Public +=
            "."_id;

        junction.Public += turf;

        junction.Variables["JUNCTION_TRACK_GRAMPA_STATS"] = "FALSE";
        junction.Variables["JUNCTION_USE_STRIPING"] = "TRUE";

        junction.configureFile("cmake/junction_config.h.in", "junction_config.h");
        junction.fileWriteOnce("junction_userconfig.h", "", true);
    }

    auto &argagg = addTarget<LibraryTarget>(s, "pvt.cppan.demo.vietjtnguyen.argagg", "0.4.6");
    {
        argagg.setChecks("argagg");
        argagg +=
            "include/.*"_rr;

        argagg.Public +=
            "include"_id;
    }

    auto &taywee_args = addTarget<LibraryTarget>(s, "pvt.cppan.demo.taywee.args", "6");
    {
        taywee_args += "args.hxx";
    }

    auto &fmt = addTarget<LibraryTarget>(s, "pvt.cppan.demo.fmt", "4");
    {
        fmt.setChecks("fmt");
        fmt +=
            "fmt/format.*"_rr,
            "fmt/ostream.*"_rr;

        fmt.Public +=
            "fmt"_id,
            "."_id;

        fmt.Private += sw::Shared, "FMT_EXPORT"_d;
        fmt.Public += sw::Shared, "FMT_SHARED"_d;
    }

    auto &flags = addTarget<LibraryTarget>(s, "pvt.cppan.demo.grisumbras.enum_flags", "master");

    auto &json = addTarget<LibraryTarget>(s, "pvt.cppan.demo.nlohmann.json", "3");

    auto &uv = addTarget<LibraryTarget>(s, "pvt.cppan.demo.libuv", "1");
    {
        uv.Private << sw::Shared << "BUILDING_UV_SHARED"_d;
        uv.Interface << sw::Shared << "USING_UV_SHARED"_d;
        uv += "src/.*"_r;
        if (s.Settings.TargetOS.Type == OSType::Windows)
        {
            uv += "src/win/.*"_rr;
            uv.Public += "iphlpapi.lib"_lib, "psapi.lib"_lib, "userenv.lib"_lib;
        }
        else
        {
            uv +=
                "src/unix/async.c",
                "src/unix/atomic-ops.h",
                "src/unix/core.c",
                "src/unix/dl.c",
                "src/unix/fs.c",
                "src/unix/getaddrinfo.c",
                "src/unix/getnameinfo.c",
                "src/unix/internal.h",
                "src/unix/loop-watcher.c",
                "src/unix/loop.c",
                "src/unix/pipe.c",
                "src/unix/poll.c",
                "src/unix/process.c",
                "src/unix/signal.c",
                "src/unix/spinlock.h",
                "src/unix/stream.c",
                "src/unix/tcp.c",
                "src/unix/thread.c",
                "src/unix/timer.c",
                "src/unix/tty.c",
                "src/unix/udp.c";

            switch (s.Settings.TargetOS.Type)
            {
            case OSType::AIX:
                break;
            case OSType::Android:
                break;
            case OSType::Macos:
                uv +=
                    "src/unix/darwin.c",
                    "src/unix/darwin-proctitle.c",
                    "src/unix/fsevents.c",
                    "src/unix/kqueue.c",
                    "src/unix/proctitle.c";
                break;
            case OSType::FreeBSD:
                break;
            case OSType::NetBSD:
                break;
            case OSType::OpenBSD:
                break;
            case OSType::SunOS:
                break;
            case OSType::Linux:
                uv +=
                    "src/unix/linux-core.c",
                    "src/unix/linux-inotify.c",
                    "src/unix/linux-syscalls.c",
                    "src/unix/linux-syscalls.h",
                    "src/unix/proctitle.c";
                break;
            }
        }
    }

    auto &pystring = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.imageworks.pystring", "1");
    pystring += "pystring.*"_rr;

    auto &ragel = addTarget<ExecutableTarget>(s, "pvt.cppan.demo.ragel", "6");
    {
        ragel += "aapl/.*"_rr;
        ragel += "ragel/.*\\.cpp"_rr;
        ragel += "ragel/.*\\.h"_rr;
        ragel += "aapl"_idir;
        ragel.writeFileOnce(ragel.BinaryPrivateDir / "config.h");
        if (s.Settings.TargetOS.Type == OSType::Windows)
            ragel.writeFileOnce(ragel.BinaryPrivateDir / "unistd.h");
    }

    auto rl = [&ragel](auto &t, const path &in)
    {
        auto o = t.BinaryDir / (in.filename().u8string() + ".cpp");

        auto c = std::make_shared<Command>();
        c->program = ragel.getOutputFile();
        c->args.push_back((t.SourceDir / in).u8string());
        c->args.push_back("-o");
        c->args.push_back(o.u8string());
        c->addInput(t.SourceDir / in);
        c->addOutput(o);
        t += o;
    };

    auto &winflexbison_common = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.lexxmark.winflexbison.common", "master");
    {
        winflexbison_common += "common/.*"_rr;
        winflexbison_common -= "common/m4/lib/regcomp.c";
        winflexbison_common -= "common/m4/lib/regexec.c";
        winflexbison_common -= ".*\\.def"_rr;
        winflexbison_common.Public += "common/m4/lib"_idir;
        winflexbison_common.Public += "common/misc"_idir;
    }

    auto &winflexbison_flex = addTarget<ExecutableTarget>(s, "pvt.cppan.demo.lexxmark.winflexbison.flex", "master");
    {
        winflexbison_flex += "flex/.*"_rr;
        winflexbison_flex -= "flex/src/libmain.c";
        winflexbison_flex -= "flex/src/libyywrap.c";
        winflexbison_flex += winflexbison_common;
    }

    auto &winflexbison_bison = addTarget<ExecutableTarget>(s, "pvt.cppan.demo.lexxmark.winflexbison.bison", "master");
    {
        //winflexbison_bison += "bison/data/[^/]*"_r;
        winflexbison_bison += "bison/data/m4sugar/.*"_rr;
        winflexbison_bison += "bison/src/.*"_rr;
        winflexbison_bison.Public += "bison/src"_idir;
        winflexbison_bison += winflexbison_common;
        winflexbison_bison.replaceInFileOnce("bison/src/config.h", "data", normalize_path(winflexbison_bison.SourceDir / "bison/data/"));
        winflexbison_bison.replaceInFileOnce("bison/src/main.c", "if (!last_divider)", "");
        winflexbison_bison.replaceInFileOnce("bison/src/main.c", "free(local_pkgdatadir);", "");
    }

    auto flex_bison = [&winflexbison_bison, &winflexbison_flex](auto &t, const path &f, const path &b, const Strings &flex_args = {}, const Strings &bison_args = {})
    {
        auto d = b.filename();
        auto bdir = t.BinaryPrivateDir / "fb" / d;

        auto o = bdir / (b.filename().u8string() + ".cpp");
        auto oh = bdir / (b.filename().u8string() + ".hpp");
        t += IncludeDirectory(oh.parent_path());

        fs::create_directories(bdir);

        {
            auto c = std::make_shared<Command>();
            c->program = winflexbison_bison.getOutputFile();
            c->working_directory = bdir;
            c->args.push_back("-o");
            c->args.push_back(o.u8string());
            c->args.push_back("--defines=" + oh.u8string());
            c->args.insert(c->args.end(), bison_args.begin(), bison_args.end());
            c->args.push_back((t.SourceDir / b).u8string());
            c->addInput(t.SourceDir / b);
            c->addOutput(o);
            c->addOutput(oh);
            t += o, oh;
        }

        {
            auto o = bdir / (f.filename().u8string() + ".cpp");

            auto c = std::make_shared<Command>();
            c->program = winflexbison_flex.getOutputFile();
            c->working_directory = bdir;
            c->args.push_back("-o");
            c->args.push_back(o.u8string());
            c->args.insert(c->args.end(), flex_args.begin(), flex_args.end());
            c->args.push_back((t.SourceDir / f).u8string());
            c->addInput(t.SourceDir / f);
            c->addInput(oh);
            c->addOutput(o);
            t += o;
        }
    };

    auto flex_bison_pair = [&flex_bison](auto &t, const String &type, const path &p)
    {
        auto name = p.filename().string();
        auto name_upper = boost::to_upper_copy(name);
        auto my_parser = name + "Parser";
        my_parser[0] = toupper(my_parser[0]);

        t.Definitions["HAVE_BISON_" + name_upper + "_PARSER"];

        Context ctx;
        ctx.addLine("#pragma once");
        ctx.addLine();
        ctx.addLine("#undef  THIS_PARSER_NAME");
        ctx.addLine("#undef  THIS_PARSER_NAME_UP");
        ctx.addLine("#undef  THIS_LEXER_NAME");
        ctx.addLine("#undef  THIS_LEXER_NAME_UP");
        ctx.addLine();
        ctx.addLine("#define THIS_PARSER_NAME       " + name);
        ctx.addLine("#define THIS_PARSER_NAME_UP    " + name_upper);
        ctx.addLine("#define THIS_LEXER_NAME        THIS_PARSER_NAME");
        ctx.addLine("#define THIS_LEXER_NAME_UP     THIS_PARSER_NAME_UP");
        ctx.addLine();
        ctx.addLine("#undef  MY_PARSER");
        ctx.addLine("#define MY_PARSER              " + my_parser);
        ctx.addLine();
        ctx.addLine("#define " + type);
        ctx.addLine("#include <primitives/helper/bison.h>");
        ctx.addLine("#undef  " + type);
        ctx.addLine();
        ctx.addLine("#include <" + name + ".yy.hpp>");

        t.writeFileOnce(t.BinaryPrivateDir / (name + "_parser.h"), ctx.getText());
        t.Definitions["HAVE_BISON_" + name_upper + "_PARSER"] = 1;

        auto f = p;
        auto b = p;
        flex_bison(t, f += ".ll", b += ".yy", { "--prefix=ll_" + name }, { "-Dapi.prefix={yy_" + name + "}" });
    };

    /// llvm

    auto &llvm_demangle = addTarget<StaticLibraryTarget>(s, "pvt.egorpugin.llvm.demangle", "master");
    {
        llvm_demangle +=
            "include/llvm/Demangle/.*"_rr,
            "lib/Demangle/.*\\.cpp"_rr,
            "lib/Demangle/.*\\.h"_rr;
    }

    auto &llvm_support_lite = addTarget<StaticLibraryTarget>(s, "pvt.egorpugin.llvm.support_lite", "master");
    {
        llvm_support_lite.setChecks("support_lite");
        llvm_support_lite +=
            "include/llvm-c/.*Types\\.h"_rr,
            "include/llvm-c/ErrorHandling.h",
            "include/llvm-c/Support.h",
            "include/llvm/ADT/.*\\.h"_rr,
            "include/llvm/Config/.*\\.cmake"_rr,
            "include/llvm/Support/.*"_rr,
            "lib/Support/.*\\.c"_rr,
            "lib/Support/.*\\.cpp"_rr,
            "lib/Support/.*\\.h"_rr,
            "lib/Support/.*\\.inc"_rr;
        llvm_support_lite -=
            "include/llvm/Support/.*def"_rr;
        llvm_support_lite.Private +=
            "lib"_id;
        llvm_support_lite.Public +=
            "include"_id;
        if (s.Settings.TargetOS.Type != OSType::Windows)
            llvm_support_lite.Private += "HAVE_PTHREAD_GETSPECIFIC"_d;
        llvm_support_lite.Public += llvm_demangle;

        llvm_support_lite += "LLVM_ENABLE_THREADS=1"_v;
        llvm_support_lite += "LLVM_HAS_ATOMICS=1"_v;
        if (s.Settings.TargetOS.Type == OSType::Windows)
            llvm_support_lite += "LLVM_HOST_TRIPLE=\"unknown-unknown-windows\""_v;
        else
        {
            llvm_support_lite += "LLVM_HOST_TRIPLE=\"unknown-unknown-unknown\""_v;
            llvm_support_lite += "LLVM_ON_UNIX=1"_v;
        }
        llvm_support_lite += "RETSIGTYPE=void"_v;

        llvm_support_lite += "LLVM_VERSION_MAJOR=0"_v;
        llvm_support_lite += "LLVM_VERSION_MINOR=0"_v;
        llvm_support_lite += "LLVM_VERSION_PATCH=1"_v;

        llvm_support_lite.configureFile("include/llvm/Config/config.h.cmake", "llvm/Config/config.h");
        llvm_support_lite.configureFile("include/llvm/Config/llvm-config.h.cmake", "llvm/Config/llvm-config.h");
        llvm_support_lite.configureFile("include/llvm/Config/abi-breaking.h.cmake", "llvm/Config/abi-breaking.h");
    }

    /// protobuf

    auto import_from_bazel = [](auto &t)
    {
        t.ImportFromBazel = true;
    };

    auto &protobuf_lite = addTarget<LibraryTarget>(s, "pvt.cppan.demo.google.protobuf.protobuf_lite", "3");
    import_from_bazel(protobuf_lite);
    protobuf_lite += "src/google/protobuf/.*\\.h"_rr;
    //protobuf_lite.Public += "src"_idir;
    protobuf_lite += sw::Shared, "LIBPROTOBUF_EXPORTS";
    protobuf_lite.Public += sw::Shared, "PROTOBUF_USE_DLLS";

    auto &protobuf = addTarget<LibraryTarget>(s, "pvt.cppan.demo.google.protobuf.protobuf", "3");
    import_from_bazel(protobuf);
    protobuf += ".*"_rr;
    protobuf += FileRegex(protobuf_lite.SourceDir, std::regex(".*"), true);
    protobuf.Public += protobuf_lite, zlib;
    //protobuf.Public += "src"_idir;
    protobuf += sw::Shared, "LIBPROTOBUF_EXPORTS";
    protobuf.Public += sw::Shared, "PROTOBUF_USE_DLLS";

    auto &protoc_lib = addTarget<LibraryTarget>(s, "pvt.cppan.demo.google.protobuf.protoc_lib", "3");
    import_from_bazel(protoc_lib);
    protoc_lib.Public += protobuf;
    protoc_lib += sw::Shared, "LIBPROTOC_EXPORTS";
    protoc_lib.Public += sw::Shared, "PROTOBUF_USE_DLLS";

    auto &protoc = addTarget<ExecutableTarget>(s, "pvt.cppan.demo.google.protobuf.protoc", "3");
    import_from_bazel(protoc);
    protoc.Public += protoc_lib;

    auto gen_pb = [&protoc](auto &t, const path &f)
    {
        auto n = f.filename().stem().u8string();
        auto d = f.parent_path();
        auto bdir = t.BinaryDir;

        auto o = bdir / n;
        auto ocpp = o;
        ocpp += ".pb.cc";
        auto oh = o;
        oh += ".pb.h";

        auto c = std::make_shared<Command>();
        c->program = protoc.getOutputFile();
        c->working_directory = bdir;
        c->args.push_back(f.u8string());
        c->args.push_back("--cpp_out=" + bdir.u8string());
        c->args.push_back("-I");
        c->args.push_back(d.u8string());
        c->args.push_back("-I");
        c->args.push_back((protoc.SourceDir / "src").u8string());
        c->addInput(f);
        c->addOutput(ocpp);
        c->addOutput(oh);
        t += ocpp, oh;
    };

    /// grpc

    auto setup_grpc = [&import_from_bazel](auto &t)
    {
        import_from_bazel(t);
        t += ".*"_rr;
        t.Public.IncludeDirectories.insert(t.SourceDir);
        t.Public.IncludeDirectories.insert(t.SourceDir / "include");
    };

    auto &grpcpp_config_proto = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpcpp_config_proto", "1");
    setup_grpc(grpcpp_config_proto);

    auto &grpc_plugin_support = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpc_plugin_support", "1");
    setup_grpc(grpc_plugin_support);
    grpc_plugin_support.Public += grpcpp_config_proto, protoc_lib;

    auto &grpc_cpp_plugin = addTarget<ExecutableTarget>(s, "pvt.cppan.demo.google.grpc.grpc_cpp_plugin", "1");
    setup_grpc(grpc_cpp_plugin);
    grpc_cpp_plugin.Public += grpc_plugin_support;

    auto gen_grpc = [&gen_pb, &grpc_cpp_plugin, &protoc](auto &t, const path &f)
    {
        gen_pb(t, f);

        auto n = f.filename().stem().u8string();
        auto d = f.parent_path();
        auto bdir = t.BinaryDir;

        auto o = bdir / n;
        auto ocpp = o;
        ocpp += ".grpc.pb.cc";
        auto oh = o;
        oh += ".grpc.pb.h";

        auto c = std::make_shared<Command>();
        c->program = protoc.getOutputFile();
        c->working_directory = bdir;
        c->args.push_back(f.u8string());
        c->args.push_back("--grpc_out=" + bdir.u8string());
        c->args.push_back("--plugin=protoc-gen-grpc=" + grpc_cpp_plugin.getOutputFile().u8string());
        c->args.push_back("-I");
        c->args.push_back(d.u8string());
        c->args.push_back("-I");
        c->args.push_back((protoc.SourceDir / "src").u8string());
        c->addInput(f);
        c->addInput(grpc_cpp_plugin.getOutputFile());
        c->addOutput(ocpp);
        c->addOutput(oh);
        t += ocpp, oh;
    };

    auto &gpr_codegen = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.gpr_codegen", "1");
    setup_grpc(gpr_codegen);
    if (s.Settings.TargetOS.Type == OSType::Windows)
        gpr_codegen.Public += "_WIN32_WINNT=0x0600"_d;

    auto &gpr_base = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.gpr_base", "1");
    setup_grpc(gpr_base);
    gpr_base.Public += gpr_codegen;

    auto &gpr = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.gpr", "1");
    setup_grpc(gpr);
    gpr.Public += gpr_base;

    auto &nanopb = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.nanopb", "0");
    nanopb += "[^/]*\\.[hc]"_rr;
    nanopb.Public += "PB_FIELD_32BIT"_d;

    auto &grpc_nanopb = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.third_party.nanopb", "1");
    grpc_nanopb += "third_party/nanopb/[^/]*\\.[hc]"_rr;
    grpc_nanopb.Public += "PB_FIELD_32BIT"_d;

    auto &atomic = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.atomic", "1");
    setup_grpc(atomic);
    atomic.Public += gpr;

    auto &grpc_codegen = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpc_codegen", "1");
    setup_grpc(grpc_codegen);
    grpc_codegen.Public += gpr_codegen;

    auto &grpc_trace = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpc_trace", "1");
    setup_grpc(grpc_trace);
    grpc_trace.Public += gpr, grpc_codegen;

    auto &inlined_vector = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.inlined_vector", "1");
    setup_grpc(inlined_vector);
    inlined_vector.Public += gpr_base;

    auto &debug_location = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.debug_location", "1");
    setup_grpc(debug_location);

    auto &ref_counted_ptr = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.ref_counted_ptr", "1");
    setup_grpc(ref_counted_ptr);
    gpr_base.Public += gpr_base;

    auto &ref_counted = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.ref_counted", "1");
    setup_grpc(ref_counted);
    ref_counted.Public += debug_location, gpr_base, grpc_trace, ref_counted_ptr;

    auto &orphanable = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.orphanable", "1");
    setup_grpc(orphanable);
    orphanable.Public += debug_location, gpr_base, grpc_trace, ref_counted_ptr;

    auto &grpc_base_c = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpc_base_c", "1");
    setup_grpc(grpc_base_c);
    grpc_base_c.Public += gpr_base, grpc_trace, inlined_vector, orphanable, ref_counted, zlib;

    auto &grpc_base = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpc_base", "1");
    setup_grpc(grpc_base);
    grpc_base.Public += grpc_base_c, atomic;

    auto &census = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.census", "1");
    setup_grpc(census);
    census.Public += grpc_base, grpc_nanopb;

    auto &grpc_client_authority_filter = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpc_client_authority_filter", "1");
    setup_grpc(grpc_client_authority_filter);
    grpc_client_authority_filter.Public += grpc_base;

    auto &grpc_deadline_filter = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpc_deadline_filter", "1");
    setup_grpc(grpc_deadline_filter);
    grpc_deadline_filter.Public += grpc_base;

    auto &grpc_client_channel = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpc_client_channel", "1");
    setup_grpc(grpc_client_channel);
    grpc_client_channel.Public += gpr_base, grpc_base, grpc_client_authority_filter, grpc_deadline_filter, inlined_vector,
        orphanable, ref_counted, ref_counted_ptr;

    auto &grpc_lb_subchannel_list = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpc_lb_subchannel_list", "1");
    setup_grpc(grpc_lb_subchannel_list);
    grpc_lb_subchannel_list.Public += grpc_base, grpc_client_channel;

    auto &grpc_lb_policy_pick_first = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpc_lb_policy_pick_first", "1");
    setup_grpc(grpc_lb_policy_pick_first);
    grpc_lb_policy_pick_first.Public += grpc_base, grpc_client_channel, grpc_lb_subchannel_list;

    auto &grpc_lb_policy_round_robin = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpc_lb_policy_round_robin", "1");
    setup_grpc(grpc_lb_policy_round_robin);
    grpc_lb_policy_round_robin.Public += grpc_lb_subchannel_list;

    auto &grpc_max_age_filter = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpc_max_age_filter", "1");
    setup_grpc(grpc_max_age_filter);
    grpc_max_age_filter.Public += grpc_base;

    auto &grpc_message_size_filter = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpc_message_size_filter", "1");
    setup_grpc(grpc_message_size_filter);
    grpc_message_size_filter.Public += grpc_base;

    auto &grpc_resolver_dns_ares = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpc_resolver_dns_ares", "1");
    setup_grpc(grpc_resolver_dns_ares);
    grpc_resolver_dns_ares.Public += grpc_base, grpc_client_channel, c_ares;

    auto &grpc_resolver_dns_native = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpc_resolver_dns_native", "1");
    setup_grpc(grpc_resolver_dns_native);
    grpc_resolver_dns_native.Public += grpc_base, grpc_client_channel;

    auto &grpc_resolver_fake = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpc_resolver_fake", "1");
    setup_grpc(grpc_resolver_fake);
    grpc_resolver_fake.Public += grpc_base, grpc_client_channel;

    auto &grpc_resolver_sockaddr = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpc_resolver_sockaddr", "1");
    setup_grpc(grpc_resolver_sockaddr);
    grpc_resolver_sockaddr.Public += grpc_base, grpc_client_channel;

    auto &grpc_server_backward_compatibility = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpc_server_backward_compatibility", "1");
    setup_grpc(grpc_server_backward_compatibility);
    grpc_server_backward_compatibility.Public += grpc_base;

    auto &grpc_server_load_reporting = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpc_server_load_reporting", "1");
    setup_grpc(grpc_server_load_reporting);
    grpc_server_load_reporting.Public += grpc_base;

    auto &grpc_http_filters = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpc_http_filters", "1");
    setup_grpc(grpc_http_filters);
    grpc_http_filters.Public += grpc_base;

    auto &grpc_transport_chttp2_alpn = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpc_transport_chttp2_alpn", "1");
    setup_grpc(grpc_transport_chttp2_alpn);
    grpc_transport_chttp2_alpn.Public += gpr;

    auto &grpc_transport_chttp2 = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpc_transport_chttp2", "1");
    setup_grpc(grpc_transport_chttp2);
    grpc_transport_chttp2.Public += gpr_base, grpc_base, grpc_http_filters, grpc_transport_chttp2_alpn;

    auto &grpc_transport_chttp2_client_connector = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpc_transport_chttp2_client_connector", "1");
    setup_grpc(grpc_transport_chttp2_client_connector);
    grpc_transport_chttp2_client_connector.Public += grpc_base, grpc_client_channel, grpc_transport_chttp2;

    auto &grpc_transport_chttp2_client_insecure = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpc_transport_chttp2_client_insecure", "1");
    setup_grpc(grpc_transport_chttp2_client_insecure);
    grpc_transport_chttp2_client_insecure.Public += grpc_base, grpc_client_channel, grpc_transport_chttp2, grpc_transport_chttp2_client_connector;

    auto &grpc_transport_chttp2_server = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpc_transport_chttp2_server", "1");
    setup_grpc(grpc_transport_chttp2_server);
    grpc_transport_chttp2_server.Public += grpc_base, grpc_transport_chttp2;

    auto &grpc_transport_chttp2_server_insecure = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpc_transport_chttp2_server_insecure", "1");
    setup_grpc(grpc_transport_chttp2_server_insecure);
    grpc_transport_chttp2_server_insecure.Public += grpc_base, grpc_transport_chttp2, grpc_transport_chttp2_server;

    auto &grpc_transport_inproc = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpc_transport_inproc", "1");
    setup_grpc(grpc_transport_inproc);
    grpc_transport_inproc.Public += grpc_base;

    auto &grpc_workaround_cronet_compression_filter = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpc_workaround_cronet_compression_filter", "1");
    setup_grpc(grpc_workaround_cronet_compression_filter);
    grpc_workaround_cronet_compression_filter.Public += grpc_server_backward_compatibility;

    auto &grpc_common = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpc_common", "1");
    setup_grpc(grpc_common);
    grpc_common.Public += census, grpc_base, grpc_client_authority_filter, grpc_deadline_filter, grpc_lb_policy_pick_first,
        grpc_lb_policy_round_robin, grpc_max_age_filter, grpc_message_size_filter, grpc_resolver_dns_ares, grpc_resolver_dns_native,
        grpc_resolver_fake, grpc_resolver_sockaddr, grpc_server_backward_compatibility, grpc_server_load_reporting, grpc_transport_chttp2_client_insecure,
        grpc_transport_chttp2_server_insecure, grpc_transport_inproc, grpc_workaround_cronet_compression_filter;

    auto &alts_proto = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.alts_proto", "1");
    setup_grpc(alts_proto);
    alts_proto.Public += nanopb;

    auto &alts_util = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.alts_util", "1");
    setup_grpc(alts_util);
    alts_util.Public += alts_proto, gpr, grpc_base;

    auto &tsi_interface = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.tsi_interface", "1");
    setup_grpc(tsi_interface);
    tsi_interface.Public += gpr, grpc_trace;

    auto &alts_frame_protector = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.alts_frame_protector", "1");
    setup_grpc(alts_frame_protector);
    alts_frame_protector.Public += gpr, grpc_base, tsi_interface, ssl;

    auto &tsi = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.tsi", "1");
    setup_grpc(tsi);
    tsi.Public += alts_frame_protector, alts_util, gpr, grpc_base, grpc_transport_chttp2_client_insecure, tsi_interface;

    auto &grpc_secure = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpc_secure", "1");
    setup_grpc(grpc_secure);
    grpc_secure.Public += alts_util, grpc_base, grpc_transport_chttp2_alpn, tsi;

    auto &grpc_lb_policy_grpclb_secure = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpc_lb_policy_grpclb_secure", "1");
    setup_grpc(grpc_lb_policy_grpclb_secure);
    grpc_lb_policy_grpclb_secure.Public += grpc_base, grpc_client_channel, grpc_resolver_fake, grpc_secure, grpc_nanopb;

    auto &grpc_transport_chttp2_client_secure = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpc_transport_chttp2_client_secure", "1");
    setup_grpc(grpc_transport_chttp2_client_secure);
    grpc_transport_chttp2_client_secure.Public += grpc_base, grpc_client_channel, grpc_secure, grpc_transport_chttp2, grpc_transport_chttp2_client_connector;

    auto &grpc_transport_chttp2_server_secure = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpc_transport_chttp2_server_secure", "1");
    setup_grpc(grpc_transport_chttp2_server_secure);
    grpc_transport_chttp2_server_secure.Public += grpc_base, grpc_secure, grpc_transport_chttp2, grpc_transport_chttp2_server;

    auto &grpc = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpc", "1");
    setup_grpc(grpc);
    grpc.Public += grpc_common, grpc_lb_policy_grpclb_secure, grpc_secure, grpc_transport_chttp2_client_secure,
        grpc_transport_chttp2_server_secure;

    auto &grpcpp_codegen_base = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpcpp_codegen_base", "1");
    setup_grpc(grpcpp_codegen_base);
    grpcpp_codegen_base.Public += grpc_codegen;

    auto &grpcpp_base = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpcpp_base", "1");
    setup_grpc(grpcpp_base);
    grpcpp_base.Public += grpc, grpcpp_codegen_base;

    auto &grpcpp_codegen_base_src = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpcpp_codegen_base_src", "1");
    setup_grpc(grpcpp_codegen_base_src);
    grpcpp_codegen_base_src.Public += grpcpp_codegen_base;

    auto &grpcpp_codegen_proto = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpcpp_codegen_proto", "1");
    setup_grpc(grpcpp_codegen_proto);
    grpcpp_codegen_proto.Public += grpcpp_codegen_base, grpcpp_config_proto;

    auto &grpcpp = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpcpp", "1");
    setup_grpc(grpcpp);
    grpcpp.Public += gpr, grpc, grpcpp_base, grpcpp_codegen_base, grpcpp_codegen_base_src, grpcpp_codegen_proto;

    /// primitives

    const path cppan2_base = path(__FILE__).parent_path().parent_path().parent_path().parent_path().parent_path();
    path primitives_base = getDirectories().storage_dir_tmp / "primitives";
    if (fs::exists("d:/dev/primitives"))
        primitives_base = "d:/dev/primitives";
    else if (!fs::exists(primitives_base))
        primitives::Command::execute({ "git", "clone", "https://github.com/egorpugin/primitives", primitives_base.u8string() });

    auto setup_primitives = [&primitives_base](auto &t)
    {
        auto n = t.getPackage().getPath().back();
        t.SourceDir = primitives_base / ("src/" + n);
        t.ApiName = "PRIMITIVES_" + boost::to_upper_copy(n) + "_API";
        t.CPPVersion = CPPLanguageStandard::CPP17;
        t += ".*"_rr; // explicit!
    };

    auto setup_primitives2 = [&primitives_base](auto &t, const path &subdir)
    {
        auto n = t.getPackage().getPath().back();
        t.SourceDir = primitives_base / ("src/" + subdir.u8string() + "/" + n);
        t.ApiName = "PRIMITIVES_" + boost::to_upper_copy(subdir.u8string() + "_" + n) + "_API";
        t.CPPVersion = CPPLanguageStandard::CPP17;
        t += ".*"_rr; // explicit!
    };

    // primitives
    auto &p_string = addTarget<LibraryTarget>(s, "pvt.egorpugin.primitives.string", "master");
    p_string.Public += boost_algorithm;
    setup_primitives(p_string);

    auto &p_filesystem = addTarget<LibraryTarget>(s, "pvt.egorpugin.primitives.filesystem", "master");
    p_filesystem.Public += p_string, boost_filesystem, boost_thread, flags, uv;
    setup_primitives(p_filesystem);

    auto &p_templates = addTarget<LibraryTarget>(s, "pvt.egorpugin.primitives.templates", "master");
    setup_primitives(p_templates);

    auto &p_context = addTarget<LibraryTarget>(s, "pvt.egorpugin.primitives.context", "master");
    setup_primitives(p_context);

    auto &p_minidump = addTarget<StaticLibraryTarget>(s, "pvt.egorpugin.primitives.minidump", "master");
    setup_primitives(p_minidump);
    if (s.Settings.TargetOS.Type == OSType::Windows)
        p_minidump.Public += "dbghelp.lib"_lib;

    auto &p_executor = addTarget<LibraryTarget>(s, "pvt.egorpugin.primitives.executor", "master");
    p_executor.Public += boost_asio, boost_system, p_templates, p_minidump;
    setup_primitives(p_executor);

    auto &p_command = addTarget<LibraryTarget>(s, "pvt.egorpugin.primitives.command", "master");
    p_command.Public += p_filesystem, p_templates, boost_process, uv;
    setup_primitives(p_command);
    if (s.Settings.TargetOS.Type == OSType::Windows)
        p_command.Public += "Shell32.lib"_l;

    auto &p_date_time = addTarget<LibraryTarget>(s, "pvt.egorpugin.primitives.date_time", "master");
    p_date_time.Public += p_string, boost_date_time;
    setup_primitives(p_date_time);

    auto &p_lock = addTarget<LibraryTarget>(s, "pvt.egorpugin.primitives.lock", "master");
    p_lock.Public += p_filesystem, boost_interprocess;
    setup_primitives(p_lock);

    auto &p_log = addTarget<LibraryTarget>(s, "pvt.egorpugin.primitives.log", "master");
    p_log.Public += boost_log;
    setup_primitives(p_log);

    auto &p_yaml = addTarget<LibraryTarget>(s, "pvt.egorpugin.primitives.yaml", "master");
    p_yaml.Public += p_string, yaml_cpp;
    setup_primitives(p_yaml);

    auto &p_pack = addTarget<LibraryTarget>(s, "pvt.egorpugin.primitives.pack", "master");
    p_pack.Public += p_filesystem, p_templates, libarchive;
    setup_primitives(p_pack);

    auto &p_http = addTarget<LibraryTarget>(s, "pvt.egorpugin.primitives.http", "master");
    p_http.Public += p_filesystem, p_templates, libcurl;
    setup_primitives(p_http);
    if (s.Settings.TargetOS.Type == OSType::Windows)
        p_http.Public += "Winhttp.lib"_l;

    auto &p_hash = addTarget<LibraryTarget>(s, "pvt.egorpugin.primitives.hash", "master");
    p_hash.Public += p_filesystem, rhash, crypto;
    setup_primitives(p_hash);

    auto &p_win32helpers = addTarget<LibraryTarget>(s, "pvt.egorpugin.primitives.win32helpers", "master");
    p_win32helpers.Public += p_filesystem, boost_dll, boost_algorithm;
    setup_primitives(p_win32helpers);
    if (s.Settings.TargetOS.Type == OSType::Windows)
        p_win32helpers.Public += "UNICODE"_d;

    auto &p_db_common = addTarget<LibraryTarget>(s, "pvt.egorpugin.primitives.db.common", "master");
    p_db_common.Public += p_filesystem, p_templates, pystring;
    setup_primitives2(p_db_common, "db");

    auto &p_db_sqlite3 = addTarget<LibraryTarget>(s, "pvt.egorpugin.primitives.db.sqlite3", "master");
    p_db_sqlite3.Public += p_db_common, sqlite3;
    setup_primitives2(p_db_sqlite3, "db");

    auto &p_error_handling = addTarget<LibraryTarget>(s, "pvt.egorpugin.primitives.error_handling", "master");
    setup_primitives(p_error_handling);

    auto &p_main = addTarget<StaticLibraryTarget>(s, "pvt.egorpugin.primitives.main", "master");
    p_main.Public += p_error_handling;
    setup_primitives(p_main);

    auto &p_settings = addTarget<LibraryTarget>(s, "pvt.egorpugin.primitives.settings", "master");
    p_settings.Public += p_yaml, p_filesystem, p_templates, llvm_support_lite;
    setup_primitives(p_settings);
    flex_bison_pair(p_settings, "LALR1_CPP_VARIANT_PARSER", "src/settings");
    flex_bison_pair(p_settings, "LALR1_CPP_VARIANT_PARSER", "src/path");

    auto &p_sw_settings = addTarget<LibraryTarget>(s, "pvt.egorpugin.primitives.sw.settings", "master");
    p_sw_settings.Public += p_settings;
    p_sw_settings.Interface += "src/sw.settings.program_name.cpp";
    setup_primitives2(p_sw_settings, "sw");

    auto &p_sw_main = addTarget<StaticLibraryTarget>(s, "pvt.egorpugin.primitives.sw.main", "master");
    p_sw_main.Public += p_main, p_sw_settings;
    setup_primitives2(p_sw_main, "sw");

    auto &p_tools_embedder = addTarget<ExecutableTarget>(s, "pvt.egorpugin.primitives.tools.embedder", "master");
    p_tools_embedder.SourceDir = primitives_base / "src" / "tools";
    p_tools_embedder += "embedder.cpp";
    p_tools_embedder.CPPVersion = CPPLanguageStandard::CPP17;
    p_tools_embedder += p_filesystem, p_sw_main;

    auto &p_tools_sqlite2cpp = addTarget<ExecutableTarget>(s, "pvt.egorpugin.primitives.tools.sqlpp11.sqlite2cpp", "master");
    p_tools_sqlite2cpp.SourceDir = primitives_base / "src" / "tools";
    p_tools_sqlite2cpp += "sqlpp11.sqlite2cpp.cpp";
    p_tools_sqlite2cpp.CPPVersion = CPPLanguageStandard::CPP17;
    p_tools_sqlite2cpp += p_filesystem, p_context, p_sw_main, sqlite3;

    auto gen_sql = [&p_tools_sqlite2cpp](auto &t, const auto &sql_file, const auto &out_file, const String &ns)
    {
        auto c = std::make_shared<Command>();
        c->program = p_tools_sqlite2cpp.getOutputFile();
        c->args.push_back(sql_file.u8string());
        c->args.push_back((t.BinaryDir / out_file).u8string());
        c->args.push_back(ns);
        c->addInput(sql_file);
        c->addOutput(t.BinaryDir / out_file);
        t += t.BinaryDir / out_file;
    };

    auto &p_version = addTarget<LibraryTarget>(s, "pvt.egorpugin.primitives.version", "master");
    p_version.Public += p_hash, p_templates, fmt, pystring;
    setup_primitives(p_version);
    rl(p_version, "src/version.rl");
    flex_bison_pair(p_version, "GLR_CPP_PARSER", "src/range");

    /// self

    // setting to local makes build files go to working dir
    //s.Local = true;

    {
        auto &support = s.addTarget<LibraryTarget>("support");
        support.CPPVersion = CPPLanguageStandard::CPP17;
        support.Public += p_http, p_hash, p_command, p_log, p_executor, boost_property_tree, boost_stacktrace, boost_dll;
        support.SourceDir = cppan2_base / "src/support";
        support += ".*"_rr;
        support.ApiName = "SW_SUPPORT_API";
        if (s.Settings.TargetOS.Type == OSType::Windows)
            support.Public += "UNICODE"_d;

        auto &protos = s.addTarget<StaticLibraryTarget>("protos");
        protos.CPPVersion = CPPLanguageStandard::CPP17;
        protos.SourceDir = cppan2_base / "src" / "protocol";
        protos += ".*"_rr;
        protos.Public += protobuf, grpcpp, p_log;
        gen_grpc(protos, protos.SourceDir / "api.proto");

        auto &manager = s.addTarget<LibraryTarget>("manager");
        manager.ApiName = "SW_MANAGER_API";
        //manager.ExportIfStatic = true;
        manager.CPPVersion = CPPLanguageStandard::CPP17;
        manager.Public += support, protos, p_yaml, p_date_time, p_lock, p_pack, json,
            boost_variant, boost_dll, p_db_sqlite3, sqlpp11_connector_sqlite3, p_version, p_win32helpers;
        manager.SourceDir = cppan2_base;
        manager += "src/manager/.*"_rr, "include/manager/.*"_rr;
        manager.Public += "include"_idir, "src/manager"_idir;
        manager.Public += "VERSION_MAJOR=0"_d;
        manager.Public += "VERSION_MINOR=3"_d;
        manager.Public += "VERSION_PATCH=0"_d;
        {
            auto c = std::make_shared<Command>();
            c->program = p_tools_embedder.getOutputFile();
            c->working_directory = manager.SourceDir / "src/manager/inserts";
            c->args.push_back((manager.SourceDir / "src/manager/inserts/inserts.cpp.in").u8string());
            c->args.push_back((manager.BinaryDir / "inserts.cpp").u8string());
            c->addInput(manager.SourceDir / "src/builder/manager/inserts.cpp.in");
            c->addOutput(manager.BinaryDir / "inserts.cpp");
            manager += manager.BinaryDir / "inserts.cpp";
        }
        gen_sql(manager, manager.SourceDir / "src/manager/inserts/packages_db_schema.sql", "db_packages.h", "db::packages");
        gen_sql(manager, manager.SourceDir / "src/manager/inserts/service_db_schema.sql", "db_service.h", "db::service");

        auto &builder = s.addTarget<LibraryTarget>("builder");
        builder.ApiName = "SW_BUILDER_API";
        //builder.ExportIfStatic = true;
        builder.CPPVersion = CPPLanguageStandard::CPP17;
        builder.Public += manager, junction;
        builder.SourceDir = cppan2_base;
        builder += "src/builder/.*"_rr, "include/builder/.*"_rr;
        builder.Public += "include"_idir, "src/builder"_idir;
        builder -= "src/builder/db_sqlite.*"_rr;

        auto &cpp_driver = s.addTarget<LibraryTarget>("driver.cpp");
        cpp_driver.ApiName = "SW_DRIVER_CPP_API";
        //cpp_driver.ExportIfStatic = true;
        cpp_driver.CPPVersion = CPPLanguageStandard::CPP17;
        cpp_driver.Public += builder, boost_assign, boost_uuid, p_context;
        cpp_driver.SourceDir = cppan2_base;
        cpp_driver += "src/driver/cpp/.*"_rr, "include/driver/cpp/.*"_rr;
        cpp_driver.Public += "include"_idir, "src/driver/cpp"_idir;
        {
            auto c = std::make_shared<Command>();
            c->program = p_tools_embedder.getOutputFile();
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

    {
        auto &s = c.addSet("support_lite");
        s.checkFunctionExists("_alloca");
        s.checkFunctionExists("__alloca");
        s.checkFunctionExists("__ashldi3");
        s.checkFunctionExists("__ashrdi3");
        s.checkFunctionExists("__chkstk");
        s.checkFunctionExists("__chkstk_ms");
        s.checkFunctionExists("__cmpdi2");
        s.checkFunctionExists("__divdi3");
        s.checkFunctionExists("__fixdfdi");
        s.checkFunctionExists("__fixsfdi");
        s.checkFunctionExists("__floatdidf");
        s.checkFunctionExists("__lshrdi3");
        s.checkFunctionExists("__main");
        s.checkFunctionExists("__moddi3");
        s.checkFunctionExists("__udivdi3");
        s.checkFunctionExists("__umoddi3");
        s.checkFunctionExists("___chkstk");
        s.checkFunctionExists("___chkstk_ms");
        s.checkIncludeExists("CrashReporterClient.h");
        s.checkIncludeExists("dirent.h");
        s.checkIncludeExists("dlfcn.h");
        s.checkIncludeExists("errno.h");
        s.checkIncludeExists("fcntl.h");
        s.checkIncludeExists("fenv.h");
        s.checkIncludeExists("histedit.h");
        s.checkIncludeExists("inttypes.h");
        s.checkIncludeExists("link.h");
        s.checkIncludeExists("linux/magic.h");
        s.checkIncludeExists("linux/nfs_fs.h");
        s.checkIncludeExists("linux/smb.h");
        s.checkIncludeExists("mach/mach.h");
        s.checkIncludeExists("malloc.h");
        s.checkIncludeExists("malloc/malloc.h");
        s.checkIncludeExists("ndir.h");
        s.checkIncludeExists("pthread.h");
        s.checkIncludeExists("signal.h");
        s.checkIncludeExists("stdint.h");
        s.checkIncludeExists("sys/dir.h");
        s.checkIncludeExists("sys/ioctl.h");
        s.checkIncludeExists("sys/mman.h");
        s.checkIncludeExists("sys/ndir.h");
        s.checkIncludeExists("sys/param.h");
        s.checkIncludeExists("sys/resource.h");
        s.checkIncludeExists("sys/stat.h");
        s.checkIncludeExists("sys/time.h");
        s.checkIncludeExists("sys/types.h");
        s.checkIncludeExists("sys/uio.h");
        s.checkIncludeExists("termios.h");
        s.checkIncludeExists("unistd.h");
        s.checkIncludeExists("unwind.h");
        s.checkIncludeExists("valgrind/valgrind.h");
        s.checkTypeSize("int64_t");
        s.checkTypeSize("size_t");
        s.checkTypeSize("uint64_t");
        s.checkTypeSize("u_int64_t");
        s.checkTypeSize("void *");
        {
            auto &c = s.checkSymbolExists("dladdr");
            c.Parameters.Includes.push_back("dlfcn.h");
        }
        {
            auto &c = s.checkSymbolExists("dlopen");
            c.Parameters.Includes.push_back("dlfcn.h");
        }
        {
            auto &c = s.checkSymbolExists("futimens");
            c.Parameters.Includes.push_back("sys/stat.h");
        }
        {
            auto &c = s.checkSymbolExists("futimes");
            c.Parameters.Includes.push_back("sys/time.h");
        }
        {
            auto &c = s.checkSymbolExists("getcwd");
            c.Parameters.Includes.push_back("unistd.h");
        }
        {
            auto &c = s.checkSymbolExists("getpagesize");
            c.Parameters.Includes.push_back("unistd.h");
        }
        {
            auto &c = s.checkSymbolExists("getrlimit");
            c.Parameters.Includes.push_back("sys/types.h");
            c.Parameters.Includes.push_back("sys/time.h");
            c.Parameters.Includes.push_back("sys/resource.h");
        }
        {
            auto &c = s.checkSymbolExists("getrusage");
            c.Parameters.Includes.push_back("sys/resource.h");
        }
        {
            auto &c = s.checkSymbolExists("gettimeofday");
            c.Parameters.Includes.push_back("sys/time.h");
        }
        {
            auto &c = s.checkSymbolExists("isatty");
            c.Parameters.Includes.push_back("unistd.h");
        }
        {
            auto &c = s.checkSymbolExists("lseek64");
            c.Parameters.Includes.push_back("sys/types.h");
            c.Parameters.Includes.push_back("unistd.h");
        }
        {
            auto &c = s.checkSymbolExists("mallctl");
            c.Parameters.Includes.push_back("malloc_np.h");
        }
        {
            auto &c = s.checkSymbolExists("mallinfo");
            c.Parameters.Includes.push_back("malloc.h");
        }
        {
            auto &c = s.checkSymbolExists("malloc_zone_statistics");
            c.Parameters.Includes.push_back("malloc/malloc.h");
        }
        {
            auto &c = s.checkSymbolExists("mkdtemp");
            c.Parameters.Includes.push_back("stdlib.h");
            c.Parameters.Includes.push_back("unistd.h");
        }
        {
            auto &c = s.checkSymbolExists("mkstemp");
            c.Parameters.Includes.push_back("stdlib.h");
            c.Parameters.Includes.push_back("unistd.h");
        }
        {
            auto &c = s.checkSymbolExists("mktemp");
            c.Parameters.Includes.push_back("stdlib.h");
            c.Parameters.Includes.push_back("unistd.h");
        }
        {
            auto &c = s.checkSymbolExists("posix_fallocate");
            c.Parameters.Includes.push_back("fcntl.h");
        }
        {
            auto &c = s.checkSymbolExists("posix_spawn");
            c.Parameters.Includes.push_back("spawn.h");
        }
        {
            auto &c = s.checkSymbolExists("pread");
            c.Parameters.Includes.push_back("unistd.h");
        }
        {
            auto &c = s.checkSymbolExists("realpath");
            c.Parameters.Includes.push_back("stdlib.h");
        }
        {
            auto &c = s.checkSymbolExists("sbrk");
            c.Parameters.Includes.push_back("unistd.h");
        }
        {
            auto &c = s.checkSymbolExists("setenv");
            c.Parameters.Includes.push_back("stdlib.h");
        }
        {
            auto &c = s.checkSymbolExists("setrlimit");
            c.Parameters.Includes.push_back("sys/resource.h");
        }
        {
            auto &c = s.checkSymbolExists("sigaltstack");
            c.Parameters.Includes.push_back("signal.h");
        }
        {
            auto &c = s.checkSymbolExists("strerror");
            c.Parameters.Includes.push_back("string.h");
        }
        {
            auto &c = s.checkSymbolExists("strerror_r");
            c.Parameters.Includes.push_back("string.h");
        }
        {
            auto &c = s.checkSymbolExists("strtoll");
            c.Parameters.Includes.push_back("stdlib.h");
        }
        {
            auto &c = s.checkSymbolExists("sysconf");
            c.Parameters.Includes.push_back("unistd.h");
        }
        {
            auto &c = s.checkSymbolExists("writev");
            c.Parameters.Includes.push_back("sys/uio.h");
        }
        {
            auto &c = s.checkSymbolExists("_chsize_s");
            c.Parameters.Includes.push_back("io.h");
        }
        {
            auto &c = s.checkSymbolExists("_Unwind_Backtrace");
            c.Parameters.Includes.push_back("unwind.h");
        }
        {
            auto &c = s.checkSymbolExists("__GLIBC__");
            c.Parameters.Includes.push_back("stdio.h");
        }
    }
}

void build_self(Solution &s)
{
    s.Settings.Native.LibrariesType = LibraryType::Static;

    /*Packages pkgs;
    auto add_to_resolve = [&pkgs](const auto &p)
    {
    auto pkg = extractFromString(p).toPackage();
    auto d = pkg.getDirSrc();
    if (!fs::exists(d))
    pkgs[pkg.ppath.toString()] = pkg;
    };
    add_to_resolve("pvt.cppan.demo.taywee.args-6.1.0");
    resolveAllDependencies(pkgs);*/

    auto o = s.Local;
    s.Local = false;

    build_other(s);

    s.Local = o;

    //resolve();

    //s.TargetsToBuild.add(*boost_targets["chrono"]);
    //s.TargetsToBuild.add(*boost_targets["filesystem"]);
    //s.TargetsToBuild.add(*boost_targets["iostreams"]);
}
