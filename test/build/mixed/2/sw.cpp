void build(Solution &s)
{
    auto &rs = s.addTarget<RustExecutable>("main.rs");
    rs += "src/.*"_rr;
}
