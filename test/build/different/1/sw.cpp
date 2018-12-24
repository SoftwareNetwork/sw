void build(Solution &s)
{
    auto &cs = s.addTarget<CSharpExecutable>("main.cs");
    cs += ".*\\.cs"_r;

    auto &rs = s.addTarget<RustExecutable>("main.rs");
    rs -= ".*\\.rs"_r;
    rs += "main.rs"; // main file

    auto &go = s.addTarget<GoExecutable>("main.go");
    go += ".*\\.go"_r;

    auto &cpp = s.addExecutable("main.cpp");
    cpp += "main.cpp";
}
