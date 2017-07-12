#include <source.h>

#include <sstream>

#define CATCH_CONFIG_RUNNER
#include <catch.hpp>

TEST_CASE("save/load", "[source]")
{
    std::istringstream s(R"xxx(
{
    "project": "pvt.cppan.demo.sqlite3",
    "cppan": "source:\r\n    fossil: https:\/\/www.sqlite.org\/src\r\n    tag: version-3.19.3\r\n\r\nversion: 3.19.3",
    "source": {
        "fossil": {
            "url": "https:\/\/www.sqlite.org\/src",
            "tag": "version-3.19.3"
        }
    },
    "version": "3.19.3"
}
)xxx");

    ptree p;
    pt::read_json(s, p);
    REQUIRE_NOTHROW(load_source(p));

    Fossil f;
    f.url = "https://www.sqlite.org/src";
    f.tag = "version-3.19.3";
    p.clear();
    REQUIRE_NOTHROW(save_source(p, f));
}

int main(int argc, char **argv)
{
    auto rc = Catch::Session().run(argc, argv);
    return rc;
}
