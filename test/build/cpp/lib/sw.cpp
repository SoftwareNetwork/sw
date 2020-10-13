void build(Solution &s)
{
    auto &t = s.addTarget<StaticLibraryTarget>("lib");
    t += ".*"_r;
}