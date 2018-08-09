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
    };

    auto m = resolve_dependencies(deps);

    Context ctx;
    Context fcalls;
    fcalls.beginFunction("void build_self_generated(Solution &s)");
    fcalls.addLine("auto sdir_old = s.SourceDir;");
    fcalls.addLine();

    String s;
    for (auto &[u, r] : m)
    {
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
