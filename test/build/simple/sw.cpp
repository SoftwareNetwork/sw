void build(Solution &s)
{
    {
        auto &e = s.addExecutable("cpp.test1");
        e.ApiName = "API";
        e += "main.c";
    }

    {
        auto &e = s.addExecutable("cpp.test2");
        e.ApiName = "API";
        e += "main.cpp";
    }
}
