void build(sw_build_t *b)
{
    auto e = sw_add_executable(b, "test1");
    sw_set_target_property(e, "API_NAME", "API");
    sw_add_target_source(e, "main.c");

    /*{
        auto &e = s.addExecutable("test2");
        e.ApiName = "API";
        e += "main.cpp";
    }*/
}
