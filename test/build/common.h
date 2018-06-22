path getDirSrc(const String &p)
{
    auto pkg = extractFromString(p);
    auto d = pkg.getDirSrc();
    if (!fs::exists(d))
        resolveAllDependencies({ {p, pkg} });
    return d;
}
