using sw;

public void build(Build? b)
{
    {
        var t = add_executable(b, "vala.test1");
        set_target_property(t, "API_NAME", "API");
        add_target_source(t, "main.c");
    }
    {
        var t = add_executable(b, "vala.test2");
        add_target_source(t, "main.cpp");
    }
}
