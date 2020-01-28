void build(Solution &s)
{
    auto &exe1 = s.addTarget<DExecutable>("exe1");
    exe1 += "a.d"_rr;
    exe1 += "b.d"_rr;

    auto &lib = s.addTarget<DStaticLibrary>("lib");
    lib += "b.d"_rr;

    auto &dll = s.addTarget<DSharedLibrary>("dll");
    dll += "c.d"_rr;

    auto &exe2 = s.addTarget<DExecutable>("exe2");
    exe2 += "a.d"_rr;
    exe2 += lib;

    return;
    // need a way below to create implib on win

    auto &exe3 = s.addTarget<DExecutable>("exe3");
    exe3 += "a.d"_rr;
    exe3 += dll;
}
