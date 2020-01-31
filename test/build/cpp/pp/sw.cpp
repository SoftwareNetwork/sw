void build(Solution &s)
{
    auto &t1 = s.add<Executable>("test");
    t1 += "src/main.cpp";
    t1.PreprocessStep = true;
}
