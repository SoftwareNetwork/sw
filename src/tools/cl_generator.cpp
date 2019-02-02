#include <primitives/context.h>
#include <primitives/main.h>
#include <primitives/yaml.h>

#include <algorithm>

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
    String function;
    String function_current;
    StringSet properties;
    int order = 0;
};

using Flags = std::map<String, Flag>;

struct Type
{
    String name;
    String parent;
    Flags flags;

    mutable bool printed = false;

    void print(primitives::CppContext &h, primitives::CppContext &cpp) const
    {
        if (printed)
            return;

        std::vector<const Flag*> flags2;
        for (auto &[k, v] : flags)
            flags2.push_back(&v);
        std::sort(flags2.begin(), flags2.end(), [](const auto &f1, const auto &f2)
        {
            return f1->order < f2->order;
        });

        auto print_flag_decl = [&](const auto &v)
        {
            h.beginBlock("CommandLineOption<" + v.type + "> " + v.name);
            if (!v.flag.empty())
                h.addLine("cl::CommandFlag{ \"" + v.flag + "\" },");
            if (!v.default_value.empty())
                h.addLine(v.default_value + ",");
            if (!v.function_current.empty())
                h.addLine("cl::CommandLineFunction<CPPLanguageStandard>{&" + v.function_current + "},");
            for (auto &p : v.properties)
            {
                if (0);
                else if (p == "input_dependency")
                    h.addLine("cl::InputDependency{},");
                else if (p == "intermediate_file")
                    h.addLine("cl::IntermediateFile{},");
                else if (p == "output_dependency")
                    h.addLine("cl::OutputDependency{},");
                else if (p == "flag_before_each_value")
                    h.addLine("cl::CommandFlagBeforeEachValue{},");
                else if (p == "config_variable")
                    h.addLine("cl::ConfigVariable{},");
                else if (p == "separate_prefix")
                    h.addLine("cl::SeparatePrefix{},");
                else
                    throw SW_RUNTIME_ERROR("unknown property: " + p);
            }
            h.endBlock(true);
            h.emptyLines(1);
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

        h.beginBlock("struct SW_DRIVER_CPP_API " + name + (parent.empty() ? "" : (" : " + parent)));
        for (auto &v : flags2)
            print_flag_decl(*v);
        h.emptyLines(1);

        h.addLine("Strings getCommandLine(const ::sw::builder::Command &c);");
        cpp.addLine("DEFINE_OPTION_SPECIALIZATION_DUMMY(" + name + ")");
        cpp.addLine();
        cpp.beginBlock("Strings " + name + "::getCommandLine(const ::sw::builder::Command &c)");
        cpp.addLine("Strings s;");
        if (!parent.empty())
            cpp.addLine("s = " + parent + "::getCommandLine(c);");
        for (auto &v : flags2)
            print_flag(*v);
        cpp.addLine("return s;");
        cpp.endBlock();
        cpp.emptyLines(1);

        h.endBlock(true);
        h.addLine("DECLARE_OPTION_SPECIALIZATION(" + name + ");");
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
        if (kv.second["order"].IsDefined())
            fl.order = kv.second["order"].template as<int>();
        if (kv.second["function"].IsDefined())
            fl.function = kv.second["function"].template as<String>();
        if (kv.second["function_current"].IsDefined())
            fl.function_current = kv.second["function_current"].template as<String>();
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
    if (argc != 4)
    {
        printf("usage: in.yml out.h out.cpp\n");
        return 1;
    }

    path in = argv[1];
    path out1 = argv[2];
    path out2 = argv[3];

    auto h = out1.extension() == ".h" ? out1 : out2;
    auto cpp = out1.extension() == ".cpp" ? out1 : out2;

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
                t.flags[u] = f.flags[u];
            }
            else if (kv.IsMap())
            {
                for (const auto &kv2 : kv)
                {
                    auto u = kv2.first.template as<String>();
                    if (f.flags.find(u) == f.flags.end())
                        throw SW_RUNTIME_ERROR("flag '" + u + "' is missing");
                    auto &f2 = t.flags[u] = f.flags[u];

                    if (kv2.second["order"].IsDefined())
                        f2.order = kv2.second["order"].template as<int>();
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
    hctx.beginNamespace("sw");

    cctx.addLine("#include \"" + h.filename().u8string() + "\"");
    cctx.addLine();
    cctx.beginNamespace("sw");

    f.print(hctx, cctx);

    hctx.endNamespace();
    cctx.endNamespace();

    write_file/*_if_different*/(h, hctx.getText());
    write_file/*_if_different*/(cpp, cctx.getText());

    return 0;
}
