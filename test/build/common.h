path getDirSrc(const String &p)
{
    auto pkg = extractFromString(p);
    auto real_pkg = resolve_dependencies({ pkg })[pkg];
    auto d = real_pkg.getDirSrc();
    /*if (!fs::exists(d))
        resolveAllDependencies({ {p, pkg} });*/
    return d;
}
