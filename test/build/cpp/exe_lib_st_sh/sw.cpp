void build(Solution &s)
{
    auto &lib = s.addTarget<LibraryTarget>("lib");
    lib.ApiName = "MY_API";
    lib += "a.*"_r;

    auto &exe = s.addTarget<ExecutableTarget>("exe");
    exe += "main.cpp";
    exe += lib;
}