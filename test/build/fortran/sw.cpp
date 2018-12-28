void build(Solution &s)
{
    auto &f = s.addTarget<FortranExecutable>("main.fortran");
    f += "main.f";
}
