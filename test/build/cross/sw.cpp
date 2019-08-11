void build(Solution &s)
{
    auto &t = s.addExecutable("test");
    t += cpp17;
    t += "main.cpp";
    t += "pub.egorpugin.primitives.sw.main-master"_dep;
}
