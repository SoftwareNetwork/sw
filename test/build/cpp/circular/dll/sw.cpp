void build(Solution &s)
{
    auto &a = s.addTarget<SharedLibraryTarget>("a");
    a.ApiName = "MY_A_API";
    a += "a.*"_r;

    auto &b = s.addTarget<SharedLibraryTarget>("b");
    b.ApiName = "MY_B_API";
    b += "b.*"_r;

    a += b;
    b += a;
}