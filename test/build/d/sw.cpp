void build(Solution &s)
{
    auto &exe1 = s.addTarget<DExecutable>("exe1");
    exe1 += "a.d"_rr;
    exe1 += "b.d"_rr;

    /*auto &dll = s.addTarget<DSharedLibrary>("dll");
    dll += "b.d"_rr;

    auto &lib = s.addTarget<DStaticLibrary>("lib");
    lib += "b.d"_rr;*/

    /*auto &d = s.addTarget<DExecutable>("exe2");
    d += "a.d"_rr;
    d += dll;*/
}
