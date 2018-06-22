void build(Solution &sln)
{
    auto &s = sln.addDirectory("demo");
    auto &p = s.addProject("gnu.nettle", "3.3.0");
    p.Source = RemoteFile("https://ftp.gnu.org/gnu/nettle/nettle-{M}.{m}.tar.gz");

    auto &eccdata = p.addTarget<ExecutableTarget>("eccdata");
    eccdata.setChecks("eccdata");

    eccdata +=
        "eccdata.c",
        "mini-gmp.c",
        "mini-gmp.h";

    eccdata -=
        "mini-gmp.c";

    auto &nettle = p.addTarget<StaticLibraryTarget>("nettle");
    nettle.InstallDirectory = "nettle";
    nettle.setChecks("nettle");

    nettle +=
        "[^/]*\\.c"_rr,
        "[^/]*\\.h"_rr,
        "version.h.in";

    nettle -=
        "aesdata.c",
        "desdata.c",
        "eccdata.c",
        "fat-arm.c",
        "fat-x86_64.c",
        "gcmdata.c",
        "sha-example.c",
        "shadata.c",
        "twofishdata.c";

    nettle.Private += "UNUSED="_d;
    nettle += eccdata;

    if (s.Settings.Native.CompilerType == CompilerType::MSVC)
    {
        nettle.Private += "alloca=_alloca"_d;
    }

    nettle += "NETTLE_USE_MINI_GMP=1"_v;
    nettle += "MAJOR_VERSION=3"_v;
    nettle += "MINOR_VERSION=3"_v;
    const auto GMP_NUMB_BITS = std::stoi(nettle.Variables["SIZEOF_UNSIGNED_LONG"]) * 8;
    nettle.Variables["GMP_NUMB_BITS"] = std::to_string(GMP_NUMB_BITS);
    nettle.configureFile("version.h.in", "version.h");
    nettle.fileWriteOnce("config.h", "", true);
    nettle.fileWriteOnce("nettle-stdint.h", "#include <stdint.h>", true);

    const std::map<int,std::vector<int>> args =
    {
        { 192,{192,7,6,GMP_NUMB_BITS} },
        { 224,{224,12,6,GMP_NUMB_BITS} },
        { 256,{256,14,6,GMP_NUMB_BITS} },
        { 384,{384,41,6,GMP_NUMB_BITS} },
        { 521,{521,56,6,GMP_NUMB_BITS} },
        { 25519,{255,14,6,GMP_NUMB_BITS} },
    };

    for (auto &[k,a] : args)
    {
        auto c = std::make_shared<Command>();
        c->program = eccdata.getOutputFile();
        for (auto &a2 : a)
            c->args.push_back(std::to_string(a2));
        nettle += c->redirectStdout(nettle.BinaryDir / ("ecc-" + std::to_string(k) + ".h"));
    }
}

void check(Checker &c)
{
    {
        auto &s = c.addSet("eccdata");
        s.checkFunctionExists("getline");
        s.checkFunctionExists("secure_getenv");
        s.checkIncludeExists("dlfcn.h");
        s.checkIncludeExists("time.h");
        s.checkTypeSize("long");
        s.checkTypeSize("size_t");
        s.checkTypeSize("uid_t");
        s.checkTypeSize("unsigned long");
        s.checkTypeSize("void *");
        s.checkLibraryFunctionExists("dl", "dlopen");
        s.checkLibraryFunctionExists("gmp", "__gmpz_powm_sec");
        s.checkSourceCompiles("HAVE_TIME_WITH_SYS_TIME", R"sw_xxx(
        #include <time.h>
        #include <sys/time.h>
        int main() {return 0;}
        )sw_xxx");
    }

    {
        auto &s = c.addSet("nettle");
        s.checkFunctionExists("getline");
        s.checkFunctionExists("secure_getenv");
        s.checkIncludeExists("dlfcn.h");
        s.checkIncludeExists("time.h");
        s.checkTypeSize("long");
        s.checkTypeSize("size_t");
        s.checkTypeSize("uid_t");
        s.checkTypeSize("unsigned long");
        s.checkTypeSize("void *");
        s.checkLibraryFunctionExists("dl", "dlopen");
        s.checkLibraryFunctionExists("gmp", "__gmpz_powm_sec");
        s.checkSourceCompiles("HAVE_TIME_WITH_SYS_TIME", R"sw_xxx(
        #include <time.h>
        #include <sys/time.h>
        int main() {return 0;}
        )sw_xxx");
    }
}