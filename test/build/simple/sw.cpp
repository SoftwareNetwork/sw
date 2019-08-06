void configure(Build &b)
{
    //auto &s = b.addSolution();
    //s.Settings.TargetOS.Type = OSType::Linux;
    //s.Settings.Native.CompilerType = CompilerType::Clang;
}

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
