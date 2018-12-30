void build(Solution &s)
{
    auto &j = s.addTarget<JavaExecutable>("main.java");
    j += "HelloWorld.java";
}
