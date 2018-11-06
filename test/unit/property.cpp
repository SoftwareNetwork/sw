#include <property.h>

#include <chrono>
#include <iostream>

#define CATCH_CONFIG_RUNNER
#include <catch2/catch.hpp>

TEST_CASE("Checking Properties", "[property]")
{
    using namespace sw;

    PropertyValue p;
    CHECK_FALSE(p);
    CHECK(p.empty());
    CHECK((p == false));
    CHECK((false == p));

    bool b = p;
    CHECK_FALSE(b);

    p = false;
    CHECK_FALSE(p.empty());
    CHECK_FALSE(p);
    CHECK((p == false));
    CHECK((false == p));

    p = true;
    CHECK(p);
    CHECK((p == true));
    CHECK((true == p));
    CHECK((p != false));
    CHECK((false != p));

    p = 8;
    CHECK(p);
    CHECK((p == 8));
    CHECK((8 == p));
    CHECK_FALSE((p != 8));
    CHECK_FALSE((8 != p));
    CHECK((p >= 8));
    CHECK((8 >= p));
    CHECK((p <= 8));
    CHECK((8 <= p));
    CHECK((p > 7));
    CHECK((9 > p));
    CHECK((p < 9));
    CHECK((7 < p));

    int r;
    CHECK(p + 8 == 16);
    CHECK((8 + p == 16));
    r = p + 8;
    CHECK(r == 16);
    r = 8 + p;
    CHECK(r == 16);

    CHECK(p - 8 == 0);
    CHECK((8 - p == 0));
    r = p - 8;
    CHECK(r == 0);
    r = 8 - p;
    CHECK(r == 0);

    CHECK(p * 8 == 64);
    CHECK((8 * p == 64));
    r = p * 8;
    CHECK(r == 64);
    r = 8 * p;
    CHECK(r == 64);

    CHECK(p / 8 == 1);
    CHECK((8 / p == 1));
    r = p / 8;
    CHECK(r == 1);
    r = 8 / p;
    CHECK(r == 1);

    p = 1.0;
    CHECK((p == 1.0));
    CHECK((1.0 == p));
    p = 1.0f;
    CHECK((p == 1.0f));
    CHECK((1.0f == p));

    std::string s = "123";

    // constructor
    p = sw::PropertyValue{ "123" };
    CHECK((s == p));
    CHECK((p == s));
    CHECK((p == "123"));
    CHECK(("123" == p));

    // assign
    p = "123";
    CHECK((s == p));
    CHECK((p == s));
    CHECK((p == "123"));
    CHECK(("123" == p));

    p = "123"s;
    b = p == "123"s;
    CHECK(b);
    b = "123"s == p;
    CHECK(b);

    s = s + p;
    CHECK(s == "123123");

    /*



    detail::Node<std::string, PropertyTreeValueType> n;

    memcmp("123", "123", 4);

    {
        PropertyTree p;
        p.add("1.2.3.4.5.6", 123);
        p.add("1", 234);

        p["1.2.3.4.5.6"] = 1;
        p["1.2.3.4.5.6"] = 1.25;
        p["1.2.3.4.5.6"] = 1.25f;
        p["1.2.3.4.5.6"] = "12321321"s;

        /*p["1.2.3.4.5.6"] == 1;
        p["1.2.3.4.5.6"] == 1.25;
        p["1.2.3.4.5.6"] == 1.25f;
        //p["1.2.3.4.5.6"] == "12321321"s;

    p.add("2", 123);
    p.add("2", 1.25);
    p.add("2", 1.25f);
    p["2"] = 1.25;
    p["2"] = 1.25f;
    }

    {
    PropertyTree2 p;
    p["1"] = 123;
    p["2"] = 1.25;
    p["3"] = 1.25f;
    p["1.1"] = 123;
    p["1.2"] = 1.25;
    p["1.3"] = 1.25f;


    p["1"] = 123;
    p["1"] = "123"s;
    //p["1"] == 'x';

    //p["1"] == 123;
    //p["1"] == 123.23423;
    p["1"] == "123"s;

    p["1.2.3.4.5.6"] = 123;
    p["1.2.3.4.5.6"] = 1.25;
    p["1.2.3.4.5.6"] = 1.25f;



    p["1"] = 1;

    int t = p["1"];
    int tt = std::get<int>(p["1"]);

    //int t = p.getInt("1");
    //int t = p["1"].getInt();




    int a = 5;
    a++;

    /*std::cout << std::get<0>(p["1"].value()) << "\n";
    std::cout << std::get<1>(p["2"].value()) << "\n";
    std::cout << std::get<1>(p["3"].value()) << "\n";
    }



    /*template <class S, class V>
    using typed_ptree = pt::basic_ptree<S, V, pt::detail::less_nocase<S>>;
    struct PropertyTree2 : private typed_ptree<std::string, PropertyTreeValueType>
    {
    data_type &operator[](const key_type &k)
    {
    static PropertyTree2 empty;
    auto &c = get_child(k, empty);
    if (c.empty())
    c = add(k, data_type());
    return c.data();
    }

    const data_type &operator[](const key_type &k) const
    {
    return get_child(k).data();
    }
    };*/


    /*template <class T, class ... Types>
    bool operator==(const std::variant<Types...> &v1, const T &v2)
    {
    static_assert(has_type<T, std::tuple<Types...>>::value);
    return std::get<T>(v1) == v2;
    }

    */
}

int main(int argc, char **argv)
{
    Catch::Session().run(argc, argv);

    return 0;
}
