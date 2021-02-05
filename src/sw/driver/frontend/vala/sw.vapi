// SPDX-License-Identifier: AGPL-3.0-or-later

// https://wiki.gnome.org/Projects/Vala/ManualBindings

[CCode]
namespace sw {
    [CCode (cname = "sw_build_t", has_type_id = false)]
    public struct Build {}

    public void* add_executable(Build? b, string name);
    public void set_target_property(void* t, string property, string value);
    public void add_target_source(void* t, string filename);
}
