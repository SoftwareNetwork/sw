void build(Solution &s)
{
    auto &lib = s.addStaticLibrary("lib");
    {
        lib += "src/lib.cpp";
        if (lib.getSettings()["feature2"] == "1")
            lib.Public += "FEATURE2"_def;
        if (lib.getSettings()["feature3"] == "1")
            lib.Public += "FEATURE3"_def;
    }

    auto &t1 = s.addExecutable("test1");
    {
        t1 += "src/main.cpp";
        t1 += lib;
    }

    auto &t2 = s.addExecutable("test2");
    {
        t2 += "src/main.cpp";
        t2 += "WANT_FEATURE2"_def;
        auto d = t2 + lib;
        d->getSettings()["feature2"] = "1";
        d->getSettings()["feature2"].setRequired();
    }

    auto &t3 = s.addExecutable("test3");
    {
        t3 += "src/main.cpp";
        t3 += "WANT_FEATURE3"_def;
        auto d = t3 + lib;
        d->getSettings()["feature3"] = "1";
        d->getSettings()["feature3"].setRequired();
    }

    auto &t4 = s.addExecutable("test4");
    {
        t4 += "src/main.cpp";
        t4 += "WANT_FEATURE2"_def;
        t4 += "WANT_FEATURE3"_def;
        auto d = t4 + lib;
        d->getSettings()["feature2"] = "1";
        d->getSettings()["feature2"].setRequired();
        d->getSettings()["feature3"] = "1";
        d->getSettings()["feature3"].setRequired();
    }
}
