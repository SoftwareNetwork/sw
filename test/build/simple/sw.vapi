// https://wiki.gnome.org/Projects/Vala/ManualBindings

[CCode (cheader_filename = "sw/driver/c.h")]
namespace sw {
    [CCode (cname = "sw_build_t", has_type_id = false)]
    public struct Build {}

    [CCode (cname = "sw_executable_target_t", has_type_id = false)]
    public struct ExecutableTarget {}

    public ExecutableTarget add_executable(Build b, string name);
}
