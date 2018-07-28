#include <package_path.h>

#include <primitives/filesystem.h>

#include <chrono>
#include <iostream>

#define CATCH_CONFIG_RUNNER
#include <catch.hpp>

using namespace sw;

TEST_CASE("Checking Paths", "[path]")
{
    SECTION("Path")
    {
        REQUIRE_NOTHROW(Path{});
        Path p1("com.ibm");
        Path p2("CoM.IBM");
        REQUIRE((p1 == p2));
        REQUIRE_FALSE((p1 < p2));
        REQUIRE_FALSE((p2 < p1));
        REQUIRE_FALSE((p1 != p2));
        REQUIRE(p1.toString() == "com.ibm");
        REQUIRE(p2.toString() == "CoM.IBM");
        REQUIRE(p1.toStringLower() == p2.toStringLower());

        Path p3("org.IBM");
        REQUIRE((p1 < p3));
        REQUIRE((p2 < p3)); // icase!
    }

    SECTION("PackagePath")
    {
        REQUIRE_NOTHROW(PackagePath{});
        PackagePath p1("com.ibm");
        PackagePath p2("CoM.IBM");
        REQUIRE((p1 == p2));
        REQUIRE_FALSE((p1 < p2));
        REQUIRE_FALSE((p2 < p1));
        REQUIRE_FALSE((p1 != p2));
        REQUIRE(p1.toString() == "com.ibm");
        REQUIRE(p2.toString() == "CoM.IBM");
        REQUIRE(p1.toStringLower() == "com.ibm");
        REQUIRE(p2.toStringLower() == "com.ibm");
        REQUIRE(p1.toStringLower() == p2.toStringLower());

        PackagePath p3("org.IBM");
        REQUIRE((p3 < p2));
        REQUIRE_FALSE((p2 < p3)); // namespace! org < com
    }
}

int main(int argc, char **argv)
{
    Catch::Session().run(argc, argv);

    return 0;
}
