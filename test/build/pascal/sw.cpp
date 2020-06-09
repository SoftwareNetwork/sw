void build(Solution &s)
{
    auto &t = s.addTarget<sw::PascalExecutable>("x");
    t += "hello.pas";
}
