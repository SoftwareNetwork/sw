void build(Solution &s)
{
    auto &lib1 = s.addLibrary("lib1");
    lib1 += "lib1.*"_rr;
    lib1.ExportAllSymbols = true;

    auto &exe1 = s.addExecutable("exe1");
    exe1 += "exe.*"_rr;
    exe1 += lib1;
}
