#include <sw/driver/target/target2.h>

void build(Solution &s)
{
    auto &e = s.add<sw::Target2>("cpp.test2");
    //e.ApiName = "API";
    e.files.insert("main.cpp");
}
