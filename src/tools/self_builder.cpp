#include <primitives/context.h>
#include <primitives/sw/main.h>
#include <primitives/sw/settings.h>
#include <resolver.h>

using namespace sw;

int main(int argc, char **argv)
{
    cl::opt<path> p(cl::Positional, cl::Required);

    cl::ParseCommandLineOptions(argc, argv);

    struct pkg_data
    {
        bool has_checks = false;
    };

    // We must keep the whole list of dependencies here
    // otherwise, driver will try to build downloaded configs
    // and enter infinite loop.

    std::unordered_map<UnresolvedPackage, pkg_data> pkgs
    {

        {{"org.sw.demo.madler.zlib", "1"}, {}},
        {{"org.sw.demo.bzip2", "1"}, {}},
        {{"org.sw.demo.sqlite3", "3"}, {}},

        {{"org.sw.demo.boost.smart_ptr", "1"}, {}},
        {{"org.sw.demo.boost.iterator", "1"}, {}},
        {{"org.sw.demo.boost.algorithm", "1"}, {}},
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
        {{"org.sw.demo.fmt", "5"}, {}},

        /// no sources were kept behind this wall
        /// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv

        {{"org.sw.demo.grisumbras.enum_flags", "master"}, {}},
        {{"org.sw.demo.nlohmann.json", "3"}, {}},
        {{"org.sw.demo.libuv", "1"}, {}},
        {{"org.sw.demo.imageworks.pystring", "1"}, {}},

        {{"org.sw.demo.ragel", "6"}, {}},

        {{"org.sw.demo.lexxmark.winflexbison.common", "master"}, {}},
        {{"org.sw.demo.lexxmark.winflexbison.flex", "master"}, {}},
        {{"org.sw.demo.lexxmark.winflexbison.bison", "master"}, {}},

        {{"org.sw.demo.google.protobuf.protobuf_lite", "3"}, {}},
        {{"org.sw.demo.google.protobuf.protobuf", "3"}, {}},
        {{"org.sw.demo.google.protobuf.protoc_lib", "3"}, {}},
        {{"org.sw.demo.google.protobuf.protoc", "3"}, {}},

        {{"pub.egorpugin.llvm_project.llvm.demangle", "master"}, {true}},
        {{"pub.egorpugin.llvm_project.llvm.support_lite", "master"}, {true}},

    };

    UnresolvedPackages deps;
    for (auto &[p,d] : pkgs)
        deps.insert(p);

    auto m = resolve_dependencies(deps);

    Context ctx;
    ctx.addLine("#define SW_PRAGMA_HEADER 1");
    ctx.addLine();

    Context build;
    build.beginFunction("void build_self_generated(Solution &s)");
    build.addLine("auto sdir_old = s.SourceDir;");
    build.addLine();

    Context check;
    check.beginFunction("void check_self_generated(Checker &c)");

    std::set<PackageVersionGroupNumber> used_gns;

    for (auto &[u, r] : m)
    {
        if (used_gns.find(r.group_number) != used_gns.end())
            continue;
        used_gns.insert(r.group_number);

        ctx.addLine("#define THIS_PREFIX \"" + r.ppath.slice(0, r.prefix).toString() + "\"");
        ctx.addLine("#define THIS_RELATIVE_PACKAGE_PATH \"" + r.ppath.slice(r.prefix).toString() + "\"");
        ctx.addLine("#define THIS_PACKAGE_PATH THIS_PREFIX \".\" THIS_RELATIVE_PACKAGE_PATH");
        ctx.addLine("#define THIS_VERSION \"" + r.version.toString() + "\"");
        ctx.addLine("#define THIS_VERSION_DEPENDENCY \"" + r.version.toString() + "\"_dep");
        ctx.addLine("#define THIS_PACKAGE THIS_PACKAGE_PATH \"-\" THIS_VERSION");
        ctx.addLine("#define THIS_PACKAGE_DEPENDENCY THIS_PACKAGE_PATH \"-\" THIS_VERSION_DEPENDENCY");
        ctx.addLine("#define build build_" + r.getVariableName());
        if (pkgs[u].has_checks)
            ctx.addLine("#define check check_" + r.getVariableName());
        ctx.addLine("#include \"" + (r.getDirSrc2() / "sw.cpp").u8string() + "\"");
        ctx.addLine();

        build.addLine("s.NamePrefix = \"" + r.ppath.slice(0, r.prefix).toString() + "\";");
        build.addLine("build_" + r.getVariableName() + "(s);");
        build.addLine();

        if (pkgs[u].has_checks)
        {
            check.addLine("check_" + r.getVariableName() + "(c);");
            check.addLine();
        }
    }

    build.addLine("s.NamePrefix.clear();");
    build.endFunction();
    check.endFunction();

    ctx += build;
    ctx += check;

    ctx.addLine("#undef build");
    ctx.addLine("#undef check");
    ctx.addLine("#undef SW_PRAGMA_HEADER");
    ctx.addLine("#undef THIS_PREFIX");
    ctx.addLine("#undef THIS_RELATIVE_PACKAGE_PATH");
    ctx.addLine("#undef THIS_PACKAGE_PATH");
    ctx.addLine("#undef THIS_VERSION");
    ctx.addLine("#undef THIS_VERSION_DEPENDENCY");
    ctx.addLine("#undef THIS_PACKAGE");
    ctx.addLine("#undef THIS_PACKAGE_DEPENDENCY");

    write_file(p, ctx.getText());

    return 0;
}
