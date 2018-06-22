void build(Solution &s)
{
    auto &p = s.addProject("xa", "1.0.0");
    auto &d = p.addDirectory("xb");
    auto &t = d.addTarget<ExecutableTarget>("x");
    t += "main.cpp";
    t += "pub.cppan2.demo.png"_dep;
}
