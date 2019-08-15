void build(Solution &s)
{
    auto &lib = s.addStaticLibrary("lib");
    lib.ApiName = "API";
    lib += "exceptions.h";
    lib += "exceptions.cpp";

    auto &t = s.addExecutable("test");
    t += "main.cpp";
    t += lib;
}
