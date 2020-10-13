void build(Solution &s)
{
    auto &dlla = s.addTarget<SharedLibraryTarget>("dll");
    dlla.ApiName = "A_API";
    dlla += "a.*"_r;

    auto &dllb = s.addTarget<SharedLibraryTarget>("dll");
    dllb.ApiName = "B_API";
    dllb += "b.*"_r;

    auto &exe = s.addTarget<ExecutableTarget>("exe");
    exe += "main.cpp";
    exe += dlla;
    exe += dllb;
}