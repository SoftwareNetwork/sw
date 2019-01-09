void build(Solution &s)
{
    auto &d = s.addTarget<DExecutable>("main.d");
    d += ".*\\.d"_rr;
}
