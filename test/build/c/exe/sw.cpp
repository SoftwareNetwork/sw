void build(Solution &s)
{
    auto &t = s.addTarget<ExecutableTarget>("exe");
    t += ".*\\.[ch]"_rr;
}