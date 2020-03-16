void build(Solution &s)
{
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
}
