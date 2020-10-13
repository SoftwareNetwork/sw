void build(Solution &s)
{
    auto &t = s.addTarget<SharedLibraryTarget>("dll");
    t += ".*"_r;
}