void build(Solution &s)
{
    auto &t = s.addExecutable("test");
    t += cpp20;
    t += "main.cpp";
    t += "pub.egorpugin.primitives.sw.main-master"_dep;
}
