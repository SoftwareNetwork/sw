void build(Solution &s)
{
    {
        auto &e = s.addExecutable("test1");
        e.ApiName = "API";
        e += "main.c";
    }

    {
        auto &e = s.addExecutable("test2");
        e.ApiName = "API";
        e += "main.cpp";
    }
}
