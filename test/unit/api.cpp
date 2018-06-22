#ifndef SW_PACKAGE_API
#define SW_PACKAGE_API
#endif

#include <sw/driver/cpp/sw.h>

void build(Solution&);

int main()
{
    Build s;
    build(s);
    return 0;
}

void build(Solution &s)
{
    {
        auto &t = s.addTarget<StaticLibraryTarget>("x");

        t += "ZLIB_DLL"_d;
        t += "ZLIB_DLL1=123"_d;
        t += "ZLIB_DLL2="_d;

        t += "api.cpp";
        t.Private += "api.cpp";

        t.Definitions["x1"] = std::to_string(5);

        t -= "api.cpp";
        t += "api/.*cpp"_r;
        t += "api/.*cpp"_rr;
        t -= "api/.*cpp"_r;
        t -= "api/.*cpp"_rr;

        int a = 5;
        a++;
    }

    {
        auto &t = s.addTarget<LibraryTarget>("x");

        t += "api.cpp";

        t.Private += "api.cpp";

        int a = 5;
        a++;
    }
}

/*
#ifndef CPPAN_PACKAGE_API
#define CPPAN_PACKAGE_API
#endif

#include <cppan.h>

void build(Solution&);

int main()
{
Solution s;
build(s);
return 0;
}

void build(Solution &s)
{
auto &t = s.addTarget<StaticLibraryTarget>("x");

// also add operator,() as alternative
t << "api.cpp";
t << ConfigurationType::Debug << "api.cpp";

t << Visibility::Private << "api.cpp";
t << Visibility::Private << ConfigurationType::Debug << "api.cpp";

// tags?
t << SourceFiles << "api.cpp";
t << SourceFiles << ConfigurationType::Debug << "api.cpp";
t << SourceFiles << ConfigurationType::Debug << Visibility::Private
<< "api.cpp"
<< "api2.cpp"
;
t << AdditionalSourceFiles << ConfigurationType::Debug << Visibility::Private << "api.cpp";

t << Definitions << "x";
t << Definitions << "x=";
t << Definitions << "x=5";

// todo
t.Definitions["x1"] = std::to_string(5);
t.Definitions["x2"] = std::to_string(5.5);
t.Definitions["x3"] = std::to_string(5.5f);
t.Definitions["x4"] = "123";
t.Debug.Definitions["x4"] = "123";

t.Private.Definitions["x4"] = "123";
t.Private.Debug.Definitions["x4"] = "123";

t.Options.Definitions["x4"] = "123";
t.Options.Debug.Definitions["x4"] = "123";
t.Options.Private.Definitions["x4"] = "123";
t.Options.Private.Debug.Definitions["x4"] = "123";
t.AdditionalOptions.Private.Debug.Definitions["x4"] = "123";

t -= "api.cpp";
t += "api/.*cpp"_r;
t += "api/.*cpp"_rr;
t -= "api/.*cpp"_r;
t -= "api/.*cpp"_rr;


{
auto &t = s.addTarget<LibraryTarget>("x");

t += "api.cpp";
t.Debug += "api.cpp";

t.Private += "api.cpp";
t.Private.Debug += "api.cpp";

t.SourceFiles.Debug += "api.cpp";
t.SourceFiles.Private += "api.cpp";
t.SourceFiles.Private.Debug += "api.cpp";
t.AdditionalSourceFiles.Private.Debug += "api.cpp";

t.Static += "api.cpp";
t.Static.Debug += "api.cpp";

t.Static.Private += "api.cpp";
t.Static.Private.Debug += "api.cpp";

t.Static.SourceFiles.Debug += "api.cpp";
t.Static.SourceFiles.Private += "api.cpp";
t.Static.SourceFiles.Private.Debug += "api.cpp";

t.Definitions["x1"] = std::to_string(5);
t.Definitions["x2"] = std::to_string(5.5);
t.Definitions["x3"] = std::to_string(5.5f);
t.Definitions["x4"] = "123";
t.Debug.Definitions["x4"] = "123";

t.Private.Definitions["x4"] = "123";
t.Private.Debug.Definitions["x4"] = "123";

t.Options.Definitions["x4"] = "123";
t.Options.Debug.Definitions["x4"] = "123";
t.Options.Private.Definitions["x4"] = "123";
t.Options.Private.Debug.Definitions["x4"] = "123";
t.AdditionalOptions.Private.Debug.Definitions["x4"] = "123";

t.Static.Definitions["x1"] = std::to_string(5);
t.Static.Definitions["x2"] = std::to_string(5.5);
t.Static.Definitions["x3"] = std::to_string(5.5f);
t.Static.Definitions["x4"] = "123";
t.Static.Debug.Definitions["x4"] = "123";

t.Static.Private.Definitions["x4"] = "123";
t.Static.Private.Debug.Definitions["x4"] = "123";

t.Static.Options.Definitions["x4"] = "123";
t.Static.Options.Debug.Definitions["x4"] = "123";
t.Static.Options.Private.Definitions["x4"] = "123";
t.Static.Options.Private.Debug.Definitions["x4"] = "123";
t.Static.AdditionalOptions.Private.Debug.Definitions["x4"] = "123";

int a = 5;
a++;
}
}
*/
