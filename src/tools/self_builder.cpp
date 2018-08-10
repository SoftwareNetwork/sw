#include <primitives/context.h>
#include <primitives/sw/main.h>
#include <primitives/sw/settings.h>
#include <resolver.h>

using namespace sw;

int main(int argc, char **argv)
{
    cl::opt<path> p(cl::Positional, cl::Required);

    cl::ParseCommandLineOptions(argc, argv);

    UnresolvedPackages deps{

        {"org.sw.demo.madler.zlib", "1"},
        {"org.sw.demo.bzip2", "1"},
        {"org.sw.demo.sqlite3", "3"},

        {"org.sw.demo.boost.smart_ptr", "1"},
        {"org.sw.demo.boost.iterator", "1"},
        {"org.sw.demo.boost.algorithm", "1"},
        {"org.sw.demo.boost.filesystem", "1"},
        {"org.sw.demo.boost.thread", "1"},
        {"org.sw.demo.boost.asio", "1"},
        {"org.sw.demo.boost.system", "1"},
        {"org.sw.demo.boost.process", "1"},
        {"org.sw.demo.boost.date_time", "1"},
        {"org.sw.demo.boost.interprocess", "1"},
        {"org.sw.demo.boost.log", "1"},
        {"org.sw.demo.boost.dll", "1"},
        {"org.sw.demo.boost.property_tree", "1"},
        {"org.sw.demo.boost.stacktrace", "1"},
        {"org.sw.demo.boost.variant", "1"},
        {"org.sw.demo.boost.assign", "1"},
        {"org.sw.demo.boost.uuid", "1"},

    };

    auto m = resolve_dependencies(deps);

    Context ctx;
    Context fcalls;
    fcalls.beginFunction("void build_self_generated(Solution &s)");
    fcalls.addLine("auto sdir_old = s.SourceDir;");
    fcalls.addLine();

    std::set<PackageVersionGroupNumber> used_gns;

    String s;
    for (auto &[u, r] : m)
    {
        if (used_gns.find(r.group_number) != used_gns.end())
            continue;
        used_gns.insert(r.group_number);

        ctx.addLine("#define build build_" + r.getVariableName());
        ctx.addLine("#include \"" + (r.getDirSrc2() / "sw.cpp").u8string() + "\"");
        ctx.addLine();

        fcalls.addLine("s.NamePrefix = \"" + r.ppath.slice(0, 3).toString() + "\";");
        fcalls.addLine("build_" + r.getVariableName() + "(s);");
        fcalls.addLine();
    }

    fcalls.addLine("s.NamePrefix.clear();");
    fcalls.endFunction();
    s += "#undef build\n";

    write_file(p, ctx.getText() + fcalls.getText());

    return 0;
}
