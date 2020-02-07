void build(Solution &s)
{
    auto &t1 = s.addExecutable("spaces");
    t1 += "src/main with spaces.cpp";

    auto &t2 = s.addExecutable("unicode");
    t2 += fs::u8path("src/main 與 unicōde привет, мир!.cpp");
}
