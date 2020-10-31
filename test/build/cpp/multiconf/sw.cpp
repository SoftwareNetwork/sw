void configure(Solution &s)
{
    //s.Settings.Native.LibrariesType = LibraryType::Static;
    //s.Settings.Native.ConfigurationType = ConfigurationType::Debug;
    //s.Settings.Native.CompilerType = CompilerType::ClangCl;
    //s.Settings.Native.CompilerType = CompilerType::Clang;
}

void build(Solution &s)
{
    auto &t9 = s.addExecutable("test9");
    t9 += cpp11;
    t9 += "src/main9.cpp";

    auto &t10 = s.addExecutable("test10");
    {
        auto c = t10.addCommand();
        c << cmd::prog(t9)
            << cmd::std_out("main4.cpp");
    }

    auto &t1 = s.addExecutable("test");
    t1 += "src/main.cpp";

    auto &t2 = s.addExecutable("test2");
    t2 += "src/main2.cpp";

    auto &l6 = s.addLibrary("lib6");
    l6.ApiName = "L6_API";
    l6 += "src/lib6.*"_rr;

    auto &t3 = s.addExecutable("test3");
    t3.CPPVersion = CPPLanguageStandard::CPP11;
    t3 += "src/main3.cpp";
    t3 += l6;

    auto &l5 = s.addLibrary("lib5");
    l5.ApiName = "L5_API";
    l5 += "src/lib5.cpp";

    auto &t4 = s.addExecutable("test4");
    {
        auto c = t4.addCommand();
        c << cmd::prog(t3)
            << cmd::std_out("main4.cpp");
    }

    auto &l7 = s.addLibrary("lib7");
    l7.ApiName = "L7_API";
    l7 += "src/lib7.c";

    auto &t5 = s.addExecutable("test5");
    t5 += "src/main5.cpp";
    t5 += l7;

    // test generated command in dep (t8) executed before main target (t6)
    {
        auto &t8 = s.addLibrary("lib8");
        {
            t8 += "src/1.txt";
            auto c = t8.addCommand();
            c << cmd::prog(t3)
                << cmd::std_out("main8.inc");
        }

        auto &t6 = s.addStaticLibrary("test6");
        t6 += "src/main6.cpp";
        t6.Public += t8;

        auto &t7 = s.addStaticLibrary("test7");
        t7 += "src/main6.cpp";
        t7 += t6;
    }
}
