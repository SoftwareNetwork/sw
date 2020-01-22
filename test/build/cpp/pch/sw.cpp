void build(Solution &s)
{
    PrecompiledHeader pch;
    pch.header = "src/1.h";

    auto &t1 = s.add<Executable>("test");
    t1 += "src/main.cpp";
    t1.addPrecompiledHeader(pch); // using api

    auto &t2 = s.addExecutable("test2");
    t2 += "src/main2.cpp";
    t2 += "src/1.h"_pch; // using suffix

    return;

    pch.force_include_pch = false;
    auto &t3 = s.addExecutable("test3");
    t3.CPPVersion = CPPLanguageStandard::CPP11;
    t3 += "src/main3.cpp";
    t3.addPrecompiledHeader(pch);

    return;

    auto &t4 = s.addExecutable("test4");
    {
        auto c = t4.addCommand();
        c << cmd::prog(t3)
            << cmd::std_out("main4.cpp");
        t4.addPrecompiledHeader(pch);
    }
}
