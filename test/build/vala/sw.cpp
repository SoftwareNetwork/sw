void build(Solution &s)
{
    auto &lib1 = s.add<ValaSharedLibrary>("lib1");
    lib1 += "lib1.vala";

    auto &t = s.add<ValaExecutable>("exe1");
    t += "exe1.vala";
    t += lib1;

    {
        auto &t = s.add<ValaStaticLibrary>("st");
        t += "sh.vala";
    }
    {
        auto &t = s.add<ValaSharedLibrary>("sh");
        t += "sh.vala";
    }
    {
        auto &t = s.add<ValaLibrary>("shst");
        t += "sh.vala";
    }
    {
        auto &t = s.add<ValaExecutable>("hw");
        t += "hw.vala";
    }

    {
        auto &t = s.add<ValaExecutable>("exe2");
        t += "exe2.vala";
    }
}
