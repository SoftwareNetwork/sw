void build(Solution &s)
{
    auto &exe1 = s.addExecutable("exe1");
    exe1.UnityBuild = true;
    exe1 += "exe.*"_rr;
}
