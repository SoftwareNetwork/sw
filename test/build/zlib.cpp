#include "common.h"

void build(Solution &s)
{
    auto &t = s.addTarget<LibraryTarget>("zlib");
    t.SourceDir = getDirSrc("org.sw.demo.madler.zlib-1.2.11");
    t << ".*\\.[hc]"_rr;
    t << sw::Shared << "ZLIB_DLL"_d;
}
