// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <database.h>
#include <resolver.h>

#include <primitives/context.h>
#include <primitives/executor.h>
#include <primitives/sw/main.h>
#include <primitives/sw/settings.h>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "self_builder");

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
    LOG_TRACE(logger, "Starting cppan...");
}

int main(int argc, char **argv)
{
    setup_log("INFO");

    // init
    auto &sdb = getServiceDatabase();

    cl::opt<path> p(cl::Positional, cl::Required);
    cl::opt<path> packages(cl::Positional, cl::Required);

    cl::ParseCommandLineOptions(argc, argv);

    // init
    Executor e(select_number_of_threads());
    getExecutor(&e);

    struct pkg_data
    {
        bool has_checks = false;
    };

    // We must keep the whole list of dependencies here
    // otherwise, driver will try to build downloaded configs
    // and enter infinite loop.

    std::vector<std::pair<UnresolvedPackage, pkg_data>> pkgs{

        {{"org.sw.demo.madler.zlib", "1"}, {}},
        {{"org.sw.demo.bzip2", "1"}, {}},
        {{"org.sw.demo.sqlite3", "3"}, {}},

        {{"org.sw.demo.boost.smart_ptr", "1"}, {}},
        {{"org.sw.demo.boost.iterator", "1"}, {}},
        {{"org.sw.demo.boost.algorithm", "1"}, {}},
        {{"org.sw.demo.boost.bimap", "1"}, {}},
        {{"org.sw.demo.boost.filesystem", "1"}, {}},
        {{"org.sw.demo.boost.thread", "1"}, {}},
        {{"org.sw.demo.boost.asio", "1"}, {}},
        {{"org.sw.demo.boost.system", "1"}, {}},
        {{"org.sw.demo.boost.process", "1"}, {}},
        {{"org.sw.demo.boost.date_time", "1"}, {}},
        {{"org.sw.demo.boost.interprocess", "1"}, {}},
        {{"org.sw.demo.boost.log", "1"}, {}},
        {{"org.sw.demo.boost.dll", "1"}, {}},
        {{"org.sw.demo.boost.property_tree", "1"}, {}},
        {{"org.sw.demo.boost.stacktrace", "1"}, {}},
        {{"org.sw.demo.boost.variant", "1"}, {}},
        {{"org.sw.demo.boost.assign", "1"}, {}},
        {{"org.sw.demo.boost.uuid", "1"}, {}},
        {{"org.sw.demo.boost.container_hash", "1"}, {}},

        {{"org.sw.demo.jbeder.yaml_cpp", "master"}, {}},
        {{"org.sw.demo.lz4", "1"}, {}},
        {{"org.sw.demo.oberhumer.lzo.lzo", "2"}, {}},

        {{"org.sw.demo.gnu.iconv.libcharset", "1"}, {true}},
        {{"org.sw.demo.gnu.iconv.libiconv", "1"}, {true}},
        {{"org.sw.demo.gnu.gettext.intl", "0"}, {true}},
        {{"org.sw.demo.gnu.gss", "1"}, {true}},

        {{"org.sw.demo.libxml2", "2"}, {true}},
        {{"org.sw.demo.xz_utils.lzma", "5"}, {true}},

        {{"org.sw.demo.gnu.nettle.nettle", "3"}, {true}},
        {{"org.sw.demo.libarchive.libarchive", "3"}, {true}},

        {{"org.sw.demo.nghttp2", "1"}, {true}},
        {{"org.sw.demo.openssl.crypto", "1.*.*.*"}, {}},
        {{"org.sw.demo.openssl.ssl", "1.*.*.*"}, {}},
        {{"org.sw.demo.libssh2", "1"}, {true}},
        {{"org.sw.demo.c_ares", "1"}, {true}},
        {{"org.sw.demo.badger.curl.libcurl", "7"}, {true}},

        {{"org.sw.demo.aleksey14.rhash", "1"}, {}},
        {{"org.sw.demo.howardhinnant.date.date", "2"}, {}},
        {{"org.sw.demo.rbock.sqlpp11", "0"}, {}},
        {{"org.sw.demo.rbock.sqlpp11_connector_sqlite3", "0"}, {}},

        {{"org.sw.demo.preshing.turf", "master"}, {}},
        {{"org.sw.demo.preshing.junction", "master"}, {}},
        {{"org.sw.demo.fmt", "*"}, {}},
        {{"org.sw.demo.microsoft.gsl", "*"}, {}},

        /// no sources were kept behind this wall
        /// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv

        {{"org.sw.demo.grisumbras.enum_flags", "master"}, {}},
        {{"org.sw.demo.nlohmann.json", "3"}, {}},
        {{"org.sw.demo.libuv", "1"}, {}},
        {{"org.sw.demo.imageworks.pystring", "1"}, {}},
        {{"org.sw.demo.facebook.zstd.zstd", "1"}, {}},

        {{"org.sw.demo.ragel", "6"}, {}},

        {{"org.sw.demo.lexxmark.winflexbison.common", "master"}, {}},
        {{"org.sw.demo.lexxmark.winflexbison.flex", "master"}, {}},
        {{"org.sw.demo.lexxmark.winflexbison.bison", "master"}, {}},

        {{"org.sw.demo.google.protobuf.protobuf_lite", "3"}, {}},
        {{"org.sw.demo.google.protobuf.protobuf", "3"}, {}},
        {{"org.sw.demo.google.protobuf.protoc_lib", "3"}, {}},
        {{"org.sw.demo.google.protobuf.protoc", "3"}, {}},

        {{"org.sw.demo.nanopb", "0"}, {}},
        {{"org.sw.demo.google.grpc.third_party.nanopb", "1"}, {}},
        {{"org.sw.demo.google.grpc.grpcpp_config_proto", "1"}, {}},
        {{"org.sw.demo.google.grpc.grpc_plugin_support", "1"}, {}},
        {{"org.sw.demo.google.grpc.grpc_cpp_plugin", "1"}, {}},
        {{"org.sw.demo.google.grpc.gpr_codegen", "1"}, {}},
        {{"org.sw.demo.google.grpc.gpr_base", "1"}, {}},
        {{"org.sw.demo.google.grpc.gpr", "1"}, {}},
        {{"org.sw.demo.google.grpc.atomic", "1"}, {}},
        {{"org.sw.demo.google.grpc.grpc_codegen", "1"}, {}},
        {{"org.sw.demo.google.grpc.grpc_trace", "1"}, {}},
        {{"org.sw.demo.google.grpc.inlined_vector", "1"}, {}},
        {{"org.sw.demo.google.grpc.debug_location", "1"}, {}},
        {{"org.sw.demo.google.grpc.ref_counted_ptr", "1"}, {}},
        {{"org.sw.demo.google.grpc.ref_counted", "1"}, {}},
        {{"org.sw.demo.google.grpc.orphanable", "1"}, {}},
        {{"org.sw.demo.google.grpc.grpc_base_c", "1"}, {}},
        {{"org.sw.demo.google.grpc.grpc_base", "1"}, {}},
        {{"org.sw.demo.google.grpc.census", "1"}, {}},
        {{"org.sw.demo.google.grpc.grpc_client_authority_filter", "1"}, {}},
        {{"org.sw.demo.google.grpc.grpc_deadline_filter", "1"}, {}},
        {{"org.sw.demo.google.grpc.grpc_client_channel", "1"}, {}},
        {{"org.sw.demo.google.grpc.grpc_lb_subchannel_list", "1"}, {}},
        {{"org.sw.demo.google.grpc.grpc_lb_policy_pick_first", "1"}, {}},
        {{"org.sw.demo.google.grpc.grpc_lb_policy_round_robin", "1"}, {}},
        {{"org.sw.demo.google.grpc.grpc_max_age_filter", "1"}, {}},
        {{"org.sw.demo.google.grpc.grpc_message_size_filter", "1"}, {}},
        {{"org.sw.demo.google.grpc.third_party.address_sorting", "1"}, {}},
        {{"org.sw.demo.google.grpc.grpc_resolver_dns_ares", "1"}, {}},
        {{"org.sw.demo.google.grpc.grpc_resolver_dns_native", "1"}, {}},
        {{"org.sw.demo.google.grpc.grpc_resolver_fake", "1"}, {}},
        {{"org.sw.demo.google.grpc.grpc_resolver_sockaddr", "1"}, {}},
        {{"org.sw.demo.google.grpc.grpc_server_backward_compatibility", "1"}, {}},
        {{"org.sw.demo.google.grpc.grpc_http_filters", "1"}, {}},
        {{"org.sw.demo.google.grpc.grpc_transport_chttp2_alpn", "1"}, {}},
        {{"org.sw.demo.google.grpc.grpc_transport_chttp2", "1"}, {}},
        {{"org.sw.demo.google.grpc.grpc_transport_chttp2_client_connector", "1"}, {}},
        {{"org.sw.demo.google.grpc.grpc_transport_chttp2_client_insecure", "1"}, {}},
        {{"org.sw.demo.google.grpc.grpc_transport_chttp2_server", "1"}, {}},
        {{"org.sw.demo.google.grpc.grpc_transport_chttp2_server_insecure", "1"}, {}},
        {{"org.sw.demo.google.grpc.grpc_transport_inproc", "1"}, {}},
        {{"org.sw.demo.google.grpc.grpc_workaround_cronet_compression_filter", "1"}, {}},
        {{"org.sw.demo.google.grpc.grpc_common", "1"}, {}},
        {{"org.sw.demo.google.grpc.alts_proto", "1"}, {}},
        {{"org.sw.demo.google.grpc.alts_util", "1"}, {}},
        {{"org.sw.demo.google.grpc.tsi_interface", "1"}, {}},
        {{"org.sw.demo.google.grpc.alts_frame_protector", "1"}, {}},
        {{"org.sw.demo.google.grpc.tsi", "1"}, {}},
        {{"org.sw.demo.google.grpc.grpc_secure", "1"}, {}},
        {{"org.sw.demo.google.grpc.grpc_lb_policy_grpclb_secure", "1"}, {}},
        {{"org.sw.demo.google.grpc.grpc_transport_chttp2_client_secure", "1"}, {}},
        {{"org.sw.demo.google.grpc.grpc_transport_chttp2_server_secure", "1"}, {}},
        {{"org.sw.demo.google.grpc.grpc", "1"}, {}},
        {{"org.sw.demo.google.grpc.grpcpp_codegen_base", "1"}, {}},
        {{"org.sw.demo.google.grpc.grpcpp_base", "1"}, {}},
        {{"org.sw.demo.google.grpc.grpcpp_codegen_base_src", "1"}, {}},
        {{"org.sw.demo.google.grpc.grpcpp_codegen_proto", "1"}, {}},
        {{"org.sw.demo.google.grpc.grpcpp", "1"}, {}},
        { {"org.sw.demo.google.grpc.health_proto", "1"}, {} },
        { {"org.sw.demo.google.grpc.grpc_lb_policy_xds_secure", "1"}, {} },
        { {"org.sw.demo.google.grpc.grpclb_proto", "1"}, {} },

        {{"pub.egorpugin.llvm_project.llvm.demangle", "master"}, {true}},
        {{"pub.egorpugin.llvm_project.llvm.support_lite", "master"}, {true}},

        {{"org.sw.demo.google.breakpad.common.windows", "master"}, {true}},
        {{"org.sw.demo.google.breakpad.client.windows.crash_generation.client", "master"}, {true}},
        {{"org.sw.demo.google.breakpad.client.windows.crash_generation.server", "master"}, {true}},
        {{"org.sw.demo.google.breakpad.client.windows.handler", "master"}, {true}},

        {{"pub.egorpugin.primitives.string", "master"}, {}},
        {{"pub.egorpugin.primitives.filesystem", "master"}, {}},
        {{"pub.egorpugin.primitives.file_monitor", "master"}, {}},
        {{"pub.egorpugin.primitives.templates", "master"}, {}},
        {{"pub.egorpugin.primitives.context", "master"}, {}},
        {{"pub.egorpugin.primitives.executor", "master"}, {}},
        {{"pub.egorpugin.primitives.command", "master"}, {}},
        {{"pub.egorpugin.primitives.date_time", "master"}, {}},
        {{"pub.egorpugin.primitives.lock", "master"}, {}},
        {{"pub.egorpugin.primitives.log", "master"}, {}},
        {{"pub.egorpugin.primitives.yaml", "master"}, {}},
        {{"pub.egorpugin.primitives.pack", "master"}, {}},
        {{"pub.egorpugin.primitives.patch", "master"}, {}},
        {{"pub.egorpugin.primitives.http", "master"}, {}},
        {{"pub.egorpugin.primitives.hash", "master"}, {}},
        {{"pub.egorpugin.primitives.win32helpers", "master"}, {}},
        {{"pub.egorpugin.primitives.db.common", "master"}, {}},
        {{"pub.egorpugin.primitives.db.sqlite3", "master"}, {}},
        {{"pub.egorpugin.primitives.error_handling", "master"}, {}},
        {{"pub.egorpugin.primitives.main", "master"}, {}},
        {{"pub.egorpugin.primitives.settings", "master"}, {}},
        {{"pub.egorpugin.primitives.sw.settings", "master"}, {}},
        {{"pub.egorpugin.primitives.sw.main", "master"}, {}},
        {{"pub.egorpugin.primitives.tools.embedder", "master"}, {}},
        {{"pub.egorpugin.primitives.tools.sqlpp11.sqlite2cpp", "master"}, {}},
        {{"pub.egorpugin.primitives.version", "master"}, {}},

        {{"org.sw.sw.client.support", "0"}, {}},
        {{"org.sw.sw.client.protos", "0"}, {}},
        {{"org.sw.sw.client.manager", "0"}, {}},
        {{"org.sw.sw.client.tools.self_builder", "0"}, {}},
        {{"org.sw.sw.client.builder", "0"}, {}},
        {{"org.sw.sw.client.driver.cpp", "0"}, {}},
    };

    primitives::CppContext ctx_packages;
    ctx_packages.beginBlock("static UnresolvedPackages required_packages");

    UnresolvedPackages deps;
    for (auto &[p, d] : pkgs)
        deps.insert(p);

    auto m = resolve_dependencies(deps);

    // write resolved deps!
    std::map<UnresolvedPackage, pkg_data> pkgs_sorted;
    for (auto &[p, d] : pkgs)
        pkgs_sorted.emplace(p, d);
    for (auto &[p, d] : pkgs_sorted)
        ctx_packages.addLine("\"" + p.resolve().toString() + "\"s,");

    ctx_packages.endBlock(true);
    write_file_if_different(packages, ctx_packages.getText());

    primitives::CppContext ctx;
    //ctx.addLine("#define SW_PRAGMA_HEADER 1");
    ctx.addLine();

    primitives::CppContext build;
    build.beginFunction("void build_self_generated(Solution &s)");
    build.addLine("auto sdir_old = s.SourceDir;");
    build.addLine();

    primitives::CppContext check;
    check.beginFunction("void check_self_generated(Checker &c)");

    std::set<PackageVersionGroupNumber> used_gns;
    Files used_dirs;
    for (auto &[u, data] : pkgs)
    {
        auto &r = m[u];
        if (used_gns.find(r.group_number) != used_gns.end())
            continue;
        used_gns.insert(r.group_number);

        /*ctx.addLine("#define THIS_PREFIX \"" + r.ppath.slice(0, r.prefix).toString() + "\"");
        ctx.addLine("#define THIS_RELATIVE_PACKAGE_PATH \"" + r.ppath.slice(r.prefix).toString() + "\"");
        ctx.addLine("#define THIS_PACKAGE_PATH THIS_PREFIX \".\" THIS_RELATIVE_PACKAGE_PATH");
        //ctx.addLine("#define THIS_VERSION \"" + r.version.toString() + "\"");
        ctx.addLine("#define THIS_VERSION_DEPENDENCY \"" + r.version.toString() + "\"_dep"); // here we use version! our packages must know exact current version
        //ctx.addLine("#define THIS_PACKAGE THIS_PACKAGE_PATH \"-\" THIS_VERSION");
        ctx.addLine("#define THIS_PACKAGE_DEPENDENCY THIS_PACKAGE_PATH \"-\" THIS_VERSION_DEPENDENCY");*/
        ctx.addLine("#define configure configure_" + r.getVariableName());
        ctx.addLine("#define build build_" + r.getVariableName());
        if (data.has_checks)
            ctx.addLine("#define check check_" + r.getVariableName());
        ctx.addLine("#include \"" + normalize_path(r.getDirSrc2() / "sw.cpp") + "\"");
        ctx.addLine();

        build.addLine("s.NamePrefix = \"" + r.ppath.slice(0, r.prefix).toString() + "\";");
        build.addLine("s.current_module = \"" + r.toString() + "\";");
        build.addLine("s.current_gn = " + std::to_string(r.group_number) + ";");
        build.addLine("build_" + r.getVariableName() + "(s);");
        build.addLine();

        if (data.has_checks)
        {
            check.addLine("c.current_gn = " + std::to_string(r.group_number) + ";");
            check.addLine("check_" + r.getVariableName() + "(c);");
            check.addLine();
        }
    }

    build.addLine("s.NamePrefix.clear();");
    build.addLine("s.current_module.clear();");
    build.addLine("s.current_gn = 0;");
    build.endFunction();
    check.addLine("c.current_gn = 0;");
    check.endFunction();

    ctx += build;
    ctx += check;

    ctx.addLine("#undef build");
    ctx.addLine("#undef check");
    ctx.addLine("#undef configure");
    //ctx.addLine("#undef SW_PRAGMA_HEADER");
    /*ctx.addLine("#undef THIS_PREFIX");
    ctx.addLine("#undef THIS_RELATIVE_PACKAGE_PATH");
    ctx.addLine("#undef THIS_PACKAGE_PATH");
    ctx.addLine("#undef THIS_VERSION");
    ctx.addLine("#undef THIS_VERSION_DEPENDENCY");
    ctx.addLine("#undef THIS_PACKAGE");
    ctx.addLine("#undef THIS_PACKAGE_DEPENDENCY");*/

    write_file(p, ctx.getText());

    return 0;
}

EXPORT_FROM_EXECUTABLE
std::string getProgramName()
{
    return PACKAGE_NAME_CLEAN;
}
