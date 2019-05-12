#include <iomanip>
#include <sstream>

#define make_name(s) make_name1(s, __LINE__)

auto make_name1(const String &s, int line)
{
    std::ostringstream ss;
    ss << std::setfill('0') << std::setw(4) << line;
    return "t" + ss.str() + "_" + s;
};

// tests
void basic_tests(Solution &s);
void deps_tests(Solution &s);

void configure(Solution &s)
{
    for (auto LT : {
        LibraryType::Static,
        LibraryType::Shared
        })
    {
        for (auto C : {
            ConfigurationType::Debug,
            ConfigurationType::MinimalSizeRelease,
            ConfigurationType::Release,
            ConfigurationType::ReleaseWithDebugInformation})
        {
            auto c1 = s.createSettings();
            c1.Native.ConfigurationType = C;
            c1.Native.LibrariesType = LT;
            s.addSettings(c1);
        }
    }
}

void build(Solution &s)
{
    basic_tests(s);
    deps_tests(s);
}

void set_dirs(Solution &s)
{
    s.setSourceDir(fs::current_path());
}

void basic_tests(Solution &s)
{
    set_dirs(s);

    // simple exe
    {
        auto &t = s.addTarget<ExecutableTarget>(make_name("exe"));
        t.setRootDirectory("cpp/exe2");
        t += ".*"_rr;
    }

    // simple exe
    {
        auto &t = s.addTarget<ExecutableTarget>(make_name("exe"));
        t.setRootDirectory("c/exe");
        t += ".*\\.[ch]"_rr;
    }

    // simple exe
    {
        auto &t = s.addTarget<ExecutableTarget>(make_name("exe"));
        t.setRootDirectory("cpp/exe");

        t.Definitions["AND_MY_STRING"] = "\"my string\"";
        t.Definitions["AND_MY_STRING1"] = "\"my string\"";
        t.Private.Definitions["AND_MY_STRING2"] = "\"my string\"";

        //t.Linker->

        //t.SourceFiles.add(std::regex(".*"));
        t += ".*\\.txt"_r;
        t += ".*\\.txt"_r;
        t += ".*\\.txt"_rr;
        t += ".*\\.cpp"_rr;
        t += ".*\\.h"_rr;
        //t -= "1/x.cpp";
        t += "1/x.cpp";
    }

    // simple dll
    {
        auto &t = s.addTarget<SharedLibraryTarget>(make_name("dll"));
        t.setRootDirectory("cpp/dll");
        t += ".*"_r;
    }

    // simple lib
    {
        auto &t = s.addTarget<StaticLibraryTarget>(make_name("lib"));
        t.setRootDirectory("cpp/lib");
        t += ".*"_r;
    }
}

void deps_tests(Solution &s)
{
    set_dirs(s);
    s.SourceDirBase /= path("cpp") / "dep" / "exe_dll";

    // simple exe+dll+api name
    {
        auto &dll = s.addTarget<SharedLibraryTarget>(make_name("dll"));
        dll.ApiName = "MY_API";
        dll += "a.*"_r;

        auto &exe = s.addTarget<ExecutableTarget>(make_name("exe"));
        exe += "main.cpp";
        exe += dll;
    }

    set_dirs(s);
    s.SourceDirBase /= path("cpp") / "dep" / "exe_lib";

    // simple exe+lib
    {
        auto &lib = s.addTarget<StaticLibraryTarget>(make_name("lib"));
        lib.ApiName = "MY_API";
        lib += "a.*"_r;

        auto &exe = s.addTarget<ExecutableTarget>(make_name("exe"));
        exe += "main.cpp";
        exe += lib;
    }

    set_dirs(s);
    s.SourceDirBase /= path("cpp") / "dep" / "exe_lib_st_sh";

    // simple exe+lib
    {
        auto &lib = s.addTarget<LibraryTarget>(make_name("lib"));
        lib.ApiName = "MY_API";
        lib += "a.*"_r;

        auto &exe = s.addTarget<ExecutableTarget>(make_name("exe"));
        exe += "main.cpp";
        exe += lib;
    }

    set_dirs(s);
    s.SourceDirBase /= path("cpp") / "dep" / "exe_dll_dll";

    // exe+dll+dll2
    {
        auto &dlla = s.addTarget<SharedLibraryTarget>(make_name("dll"));
        dlla.ApiName = "A_API";
        dlla += "a.*"_r;

        auto &dllb = s.addTarget<SharedLibraryTarget>(make_name("dll"));
        dllb.ApiName = "B_API";
        dllb += "b.*"_r;

        auto &exe = s.addTarget<ExecutableTarget>(make_name("exe"));
        exe += "main.cpp";
        exe += dlla;
        exe += dllb;
    }

    set_dirs(s);
    s.SourceDirBase /= path("cpp") / "dep" / "circular" / "dll";

    // circular dependencies test (dll+dll)
    {
        auto &a = s.addTarget<SharedLibraryTarget>(make_name("dll"));
        a.ApiName = "MY_A_API";
        a += "a.*"_r;

        auto &b = s.addTarget<SharedLibraryTarget>(make_name("dll"));
        b.ApiName = "MY_B_API";
        b += "b.*"_r;

        a += b;
        b += a;
    }

    set_dirs(s);
    s.SourceDirBase /= path("cpp") / "dep" / "circular" / "exe";

    // circular dependencies test (exe+exe)
    {
        auto &a = s.addTarget<ExecutableTarget>(make_name("exe"));
        a.ApiName = "MY_A_API";
        a += "a.*"_r;

        auto &b = s.addTarget<ExecutableTarget>(make_name("exe"));
        b.ApiName = "MY_B_API";
        b += "b.*"_r;

        a += b;
        b += a;
    }

    set_dirs(s);
    s.SourceDirBase /= path("cpp") / "dep" / "circular" / "exe_dll";

    // circular dependencies test (exe+dll)
    {
        auto &a = s.addTarget<ExecutableTarget>(make_name("exe"));
        a.ApiName = "MY_A_API";
        a += "a.*"_r;

        auto &b = s.addTarget<SharedLibraryTarget>(make_name("dll"));
        b.ApiName = "MY_B_API";
        b += "b.*"_r;

        a += b;
        b += a;
    }
}
