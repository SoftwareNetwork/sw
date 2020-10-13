void build(Solution &s)
{
    auto &dll = s.addTarget<SharedLibraryTarget>("dll");
    dll.ApiName = "MY_API";
    dll += "a.*"_r;

    auto &exe = s.addTarget<ExecutableTarget>("exe");
    exe += "main.cpp";
    exe += dll;
}