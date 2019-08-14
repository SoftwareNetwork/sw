void build(Solution &s)
{
    auto &lib = s.addStaticLibrary("lib");
    lib += "f.cpp";

    auto &t = s.addExecutable("test");
    t += "main.cpp";
    t += lib;
}
