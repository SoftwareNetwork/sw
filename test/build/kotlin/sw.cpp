void build(Solution &s)
{
    auto &k = s.addTarget<KotlinExecutable>("main.kotlin");
    k += "hello.kt";
}
