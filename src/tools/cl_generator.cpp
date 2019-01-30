#include <primitives/context.h>
#include <primitives/yaml.h>
#include <primitives/sw/main.h>
#include <primitives/sw/settings.h>

template <class ... Args>
void both(primitives::CppContext &hctx, primitives::CppContext &cctx, Args && ... args)
{
    hctx.addLine(args...);
    cctx.addLine(args...);
};

struct Flag
{
    String name;
    String flag;
    String type;
    String default_value;
    StringSet properties;
};

using Flags = std::map<String, Flag>;

struct Type
{
    String name;
    String parent;
    Flags flags;
    std::map<String, Flag> using_flags;

    mutable bool printed = false;

    void print(primitives::CppContext &h, primitives::CppContext &cpp) const
    {
        if (printed)
            return;

        auto print_flag_decl = [&](const auto &v)
        {
            h.addLine(v.type + " " + v.name);
            if (!v.default_value.empty())
                h.addText("{ " + v.default_value + " }");
            h.addText(";");
        };

        auto print_flag = [&](const auto &v)
        {
            if (!v.type.empty())
            {
                if (0);
                else if (v.type == "bool")
                {
                    cpp.increaseIndent("if (" + v.name + ")");
                    cpp.addLine("s.push_back(\"-" + v.flag + "\");");
                    cpp.decreaseIndent();
                }
                else if (v.type == "path")
                {
                }
                //else
                    //throw SW_RUNTIME_ERROR("unknown cpp type: " + v.type);
            }
        };

        h.beginBlock("struct " + name + (parent.empty() ? "" : (" : " + parent)));
        for (auto &[k, v] : flags)
            print_flag_decl(v);
        for (auto &[k, v] : using_flags)
            print_flag_decl(v);
        h.emptyLines(1);

        h.addLine("Strings getCommandLine(const ::sw::builder::Command &c);");
        cpp.beginBlock("Strings " + name + "::getCommandLine(const ::sw::builder::Command &c)");
        cpp.addLine("Strings s;");
        if (!parent.empty())
            cpp.addLine("s = " + parent + "::getCommandLine(c);");
        for (auto &[k, v] : flags)
            print_flag(v);
        for (auto &[k, v] : using_flags)
            print_flag(v);
        cpp.addLine("return s;");
        cpp.endBlock();
        cpp.emptyLines(1);

        h.endBlock(true);
        h.emptyLines(1);

        printed = true;
    }
};

using Types = std::map<String, Type>;

struct File
{
    Flags flags;
    Types types;

    void print_type(const Type &t, primitives::CppContext &h, primitives::CppContext &cpp) const
    {
        if (!t.parent.empty())
        {
            for (const auto &[k, v] : types)
                if (v.name == t.parent)
                    print_type(v, h, cpp);
        }
        t.print(h, cpp);
    }

    void print(primitives::CppContext &h, primitives::CppContext &cpp) const
    {
        for (const auto &[k, v] : types)
            print_type(v, h, cpp);
    }
};

void read_flags(const yaml &root, Flags &flags)
{
    get_map_and_iterate(root, "flags", [&flags](const auto &kv)
    {
        Flag fl;
        if (kv.second["name"].IsDefined())
            fl.name = kv.second["name"].template as<String>();
        else
            throw SW_RUNTIME_ERROR("missing name field");
        if (kv.second["flag"].IsDefined())
            fl.flag = kv.second["flag"].template as<String>();
        if (kv.second["type"].IsDefined())
            fl.type = kv.second["type"].template as<String>();
        if (kv.second["default"].IsDefined())
            fl.default_value = kv.second["default"].template as<String>();
        get_sequence_and_iterate(kv.second, "properties", [&fl](const auto &kv)
        {
            fl.properties.insert(kv.template as<String>());
        });
        auto fn = kv.first.template as<String>();
        if (flags.find(fn) != flags.end())
            throw SW_RUNTIME_ERROR("flag '" + fn + "' already used");
        flags[fn] = fl;
    });
}

int main(int argc, char **argv)
{
    cl::opt<path> in(cl::Positional, cl::Required);
    cl::opt<path> out1(cl::Positional, cl::Required);
    cl::opt<path> out2(cl::Positional, cl::Required);

    cl::ParseCommandLineOptions(argc, argv);

    auto h = out1.getValue().extension() == ".h" ? out1.getValue() : out2.getValue();
    auto cpp = out1.getValue().extension() == ".cpp" ? out1.getValue() : out2.getValue();

    auto root = YAML::Load(read_file(in));

    File f;
    read_flags(root, f.flags);

    get_map_and_iterate(root, "types", [&f](const auto &kv)
    {
        Type t;
        if (kv.second["name"].IsDefined())
            t.name = kv.second["name"].template as<String>();
        else
            throw SW_RUNTIME_ERROR("missing name field");
        if (kv.second["parent"].IsDefined())
            t.parent = kv.second["parent"].template as<String>();
        read_flags(kv.second, t.flags);

        get_sequence_and_iterate(kv.second, "using", [&f, &t](const auto &kv)
        {
            if (kv.IsScalar())
            {
                auto u = kv.template as<String>();
                if (f.flags.find(u) == f.flags.end())
                    throw SW_RUNTIME_ERROR("flag '" + u + "' is missing");
                t.using_flags[u] = f.flags[u];
            }
            else if (kv.IsMap())
            {
                for (const auto &kv2 : kv)
                {
                    auto u = kv2.first.template as<String>();
                    if (f.flags.find(u) == f.flags.end())
                        throw SW_RUNTIME_ERROR("flag '" + u + "' is missing");
                    t.using_flags[u] = f.flags[u];
                    // TODO: handle map
                }
            }
        });

        auto tn = kv.first.template as<String>();
        if (f.types.find(tn) != f.types.end())
            throw SW_RUNTIME_ERROR("type '" + tn + "' already used");
        f.types[tn] = t;
    });

    primitives::CppContext hctx, cctx;

    both(hctx, cctx, "// generated file, do not edit");
    both(hctx, cctx);

    hctx.addLine("#pragma once");
    hctx.addLine();

    cctx.addLine("#include \"" + h.filename().u8string() + "\"");
    cctx.addLine();

    f.print(hctx, cctx);

    write_file(h, hctx.getText());
    write_file(cpp, cctx.getText());

    return 0;
}
