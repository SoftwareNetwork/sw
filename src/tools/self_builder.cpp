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

    std::unordered_map<UnresolvedPackage, pkg_data> pkgs{

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

    };

    UnresolvedPackages deps;
    for (auto &[p,d] : pkgs)
        deps.insert(p);

    auto m = resolve_dependencies(deps);

    Context ctx;
    Context build;
    build.beginFunction("void build_self_generated(Solution &s)");
    build.addLine("auto sdir_old = s.SourceDir;");
    build.addLine();

    Context check;
    check.beginFunction("void check_self_generated(Checker &c)");

    std::set<PackageVersionGroupNumber> used_gns;

    String s;
    for (auto &[u, r] : m)
    {
        if (used_gns.find(r.group_number) != used_gns.end())
            continue;
        used_gns.insert(r.group_number);

        ctx.addLine("#define build build_" + r.getVariableName());
        if (pkgs[u].has_checks)
            ctx.addLine("#define check check_" + r.getVariableName());
        ctx.addLine("#include \"" + (r.getDirSrc2() / "sw.cpp").u8string() + "\"");
        ctx.addLine();

        build.addLine("s.NamePrefix = \"" + r.ppath.slice(0, 3).toString() + "\";");
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

    s += "#undef build\n";
    s += "#undef check\n";

    write_file(p, ctx.getText() + build.getText() + check.getText());

    return 0;
}
