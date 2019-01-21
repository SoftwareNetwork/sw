void configure(Solution &s)
{
    //s.Settings.Native.LibrariesType = LibraryType::Static;
    //s.Settings.Native.ConfigurationType = ConfigurationType::Debug;
    //s.Settings.Native.CompilerType = CompilerType::ClangCl;
    //s.Settings.Native.CompilerType = CompilerType::Clang;
}

void build(Solution &s)
{
    auto &t1 = s.add<Executable>("test");
    t1 += "src/main.cpp";
    //t1 += "org.sw.demo.boost.log-develop"_dep;

    auto &t2 = s.addExecutable("test2");
    t2 += "src/main2.cpp";

    auto &t3 = s.addExecutable("test3");
    t3.CPPVersion = CPPLanguageStandard::CPP11;
    t3 += "src/main3.cpp";

    auto &t4 = s.addExecutable("test4");
    {
        auto c = t4.addCommand();
        c << cmd::prog(t3)
            << cmd::std_out("main4.cpp");
    }

    //auto &t3 = s.addExecutable("test3");
    //t3 += "src/a/main3.cpp";
}
