void build(Solution &s)
{
    auto &t1 = s.add<Executable>("test");
    t1 += "src/main.cpp";
    t1 += "src/1.h"_pch; // relative

    auto &t2 = s.addExecutable("test2");
    t2 += "src/main2.cpp";
    t2 += PrecompiledHeader(t2.SourceDir / "src/1.h"); // full path

    auto &t3 = s.addExecutable("test3");
    t3.CPPVersion = CPPLanguageStandard::CPP11;
    t3 += "src/main3.cpp";
    t3 += "src/1.h"_pch;

    auto &t4 = s.addExecutable("test4");
    {
        auto c = t4.addCommand();
        c << cmd::prog(t3)
            << cmd::std_out("main4.cpp");
        t4 += "src"_idir;
        t4 += "src/2"_idir;
        t4 += "<1.h>"_pch; // relative & angle brackets
        t4 += "<2.h>"_pch; // relative & angle brackets
        t4 += "<fstream>"_pch; // std header & angle brackets
    }
}
