void build(Solution &s)
{
    auto &exe1 = s.addExecutable("exe1");
    exe1 += "png.cpp";
    exe1 += "org.sw.demo.glennrp.png"_dep;
}
