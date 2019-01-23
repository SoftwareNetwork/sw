void build(Solution &s)
{
    PrecompiledHeader pch;
    pch.header = "src/1.h";
    pch.force_include_pch = true;

    auto &t1 = s.add<Executable>("test");
    t1 += "src/main.cpp";
    t1.addPrecompiledHeader(pch);

    auto &t2 = s.addExecutable("test2");
    t2 += "src/main2.cpp";
    t2.addPrecompiledHeader(pch);

    auto &t3 = s.addExecutable("test3");
    t3.CPPVersion = CPPLanguageStandard::CPP11;
    t3 += "src/main3.cpp";
    t3.addPrecompiledHeader(pch);

    auto &t4 = s.addExecutable("test4");
    {
        auto c = t4.addCommand();
        c << cmd::prog(t3)
            << cmd::std_out("main4.cpp");
        t4.addPrecompiledHeader(pch);
    }
}
