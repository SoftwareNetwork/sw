void build(Solution &s)
{
    auto &t = s.addExecutable("x");
    {
        t.setExtensionProgram(".asm", "org.sw.demo.yasm"_dep);
        t += "common.asm";
        t += "user32.lib"_slib;
        t.LinkOptions.push_back("/LARGEADDRESSAWARE:NO");
    }
}
