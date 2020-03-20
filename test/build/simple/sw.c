void build(sw_build_t *b)
{
    {
        sw_target_t *e = sw_add_executable(b, "c.test1");
        sw_set_target_property(e, "API_NAME", "API");
        sw_add_target_source(e, "main.c");
    }

    {
        sw_target_t *e = sw_add_executable(b, "c.test2");
        sw_add_target_source(e, "main.cpp");
    }
}
