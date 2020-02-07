void build(Solution &s)
{
    auto &lib1 = s.addLibrary("lib1");
    lib1.ApiName = "LIB1_API";
    lib1 += "lib1.*"_rr;

    auto &lib2 = s.addLibrary("lib2");
    lib2.ApiName = "LIB2_API";
    lib2 += "lib2.*"_rr;
    lib2 += lib1;

    auto &lib3 = s.addLibrary("lib3");
    lib3.ApiName = "LIB3_API";
    lib3 += "lib3.*"_rr;
    lib3 += lib2;

    auto &exe1 = s.addExecutable("exe1");
    exe1 += "exe.*"_rr;
    exe1 += lib3;
}
