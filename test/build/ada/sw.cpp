void build(Solution &s)
{
    auto &t = s.addTarget<sw::AdaExecutable>("x");
    t += "hello.adb";
}
