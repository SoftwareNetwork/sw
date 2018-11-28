void configure(Solution &s)
{
    auto &s1 = s.addSolution();
    s1.Settings.Native.LibrariesType = LibraryType::Static;

    auto &s2 = s.addSolution();
    s2.Settings.Native.LibrariesType = LibraryType::Shared;
}

void build(Solution &s)
{
    auto &lib1 = s.addLibrary("lib1");
    lib1.ApiName = "LIB1_API";
    lib1 += "lib1.*"_rr;

    auto &lib2 = s.addLibrary("lib2");
    lib2.ApiName = "LIB2_API";
    lib2 += "lib2.*"_rr;
    lib2 += lib1;

    auto &exe1 = s.addExecutable("exe1");
    exe1 += "exe.*"_rr;
    exe1 += lib2;
}
