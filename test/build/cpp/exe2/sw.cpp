void build(Solution &s)
{
    auto &t = s.addTarget<ExecutableTarget>("exe");
    t += ".*"_rr;
}