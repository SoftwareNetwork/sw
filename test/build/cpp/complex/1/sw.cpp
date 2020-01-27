void build(Solution &s)
{
    auto &p = s.addProject("xa", "1.0.0");
    auto &d = p.addDirectory("xb");
    auto &t = d.addTarget<ExecutableTarget>("x");
    t += "main.cpp";
    t += "org.sw.demo.glennrp.png"_dep;
}
