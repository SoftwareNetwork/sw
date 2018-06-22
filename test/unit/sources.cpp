#ifndef CPPAN_PACKAGE_API
#define CPPAN_PACKAGE_API
#endif

// builder stuff
#include <solution.h>
#include <suffix.h>

#include <chrono>
#include <iostream>

#define CATCH_CONFIG_RUNNER
#include <catch.hpp>

#define make_name(s) make_name1(s, __LINE__)
#define make_test_name() make_name("test")

using namespace sw;

auto make_name1(const String &s, int line)
{
    std::ostringstream ss;
    ss << std::setfill('0') << std::setw(4) << line;
    return "t" + ss.str() + "_" + s;
};

TEST_CASE("Checking adding of source files", "[add]")
{
    Build s;

    SECTION("single add")
    {
        auto &t = s.add<LibraryTarget>(make_test_name());
        t += "unit/api.cpp";
        REQUIRE(t.size() == 1);
        t += "unit/sources.cpp";
        REQUIRE(t.size() == 2);
        REQUIRE(t.sizeKnown() == 2);
        REQUIRE(t.sizeSkipped() == 0);

        t -= "unit/api.cpp";
        REQUIRE(t.size() == 2);
        REQUIRE(t.sizeKnown() == 1);
        REQUIRE(t.sizeSkipped() == 1);
        t -= "unit/sources.cpp";
        REQUIRE(t.size() == 2);
        REQUIRE(t.sizeKnown() == 0);
        REQUIRE(t.sizeSkipped() == 2);
        t -= "unit/NOT_EXISTENT_FILE.cpp";
        REQUIRE(t.size() == 2);
        REQUIRE(t.sizeKnown() == 0);
        REQUIRE(t.sizeSkipped() == 2);
    }

    SECTION("regex")
    {
        auto &t = s.add<LibraryTarget>(make_test_name());
        t += "unit/.*cpp"_r;
        REQUIRE(t.size() == 2);
    }

    SECTION("recursive regex")
    {
        auto &t = s.add<LibraryTarget>(make_test_name());
        t += "unit/.*cpp"_rr;
        REQUIRE(t.size() == 2);
    }

    SECTION("recursive regex with not existing subdir")
    {
        auto &t = s.add<LibraryTarget>(make_test_name());
        t += "unit/x/.*cpp"_r;
        REQUIRE(t.size() == 0);
    }
}

int main(int argc, char **argv)
{
    setup_utf8_filesystem();

    Catch::Session().run(argc, argv);

    return 0;
}
