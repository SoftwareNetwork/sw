#include "cl_generator.h"

#include <primitives/main.h>
#include <primitives/yaml.h>

#include <algorithm>

template <class ... Args>
void both(primitives::CppEmitter &hctx, primitives::CppEmitter &cctx, Args && ... args)
{
    hctx.addLine(args...);
    cctx.addLine(args...);
};

String EnumValue::getIdeName() const
{
    if (!ide_name.empty())
        return ide_name;
    return name;
}

String Flag::getTypeWithNs() const
{
    String s = ns;
    if (!s.empty())
        s += "::";
    s += type;
    return s;
}

String Flag::getIdeName() const
{
    if (!ide_name.empty())
        return ide_name;
    return name;
}

void Flag::printDecl(primitives::CppEmitter &ctx) const
{
    if (disabled)
        return;

    ctx.beginBlock("CommandLineOption<" + getTypeWithNs() + "> " + name);
    if (!flag.empty())
        ctx.addLine("cl::CommandFlag{ \"" + flag + "\" },");
    if (!default_value.empty())
    {
        ctx.addLine(ns);
        if (!ns.empty())
            ctx.addText("::");
        if (!enum_vals.empty())
            ctx.addText(type + "::");
        ctx.addText(default_value + ",");
    }
    if (!function_current.empty())
        ctx.addLine("cl::CommandLineFunction<CPPLanguageStandard>{&" + function_current + "},");
    for (auto &p : properties)
    {
        if (0);
        else if (p == "input_dependency")
            ctx.addLine("cl::InputDependency{},");
        else if (p == "intermediate_file")
            ctx.addLine("cl::IntermediateFile{},");
        else if (p == "output_dependency")
            ctx.addLine("cl::OutputDependency{},");
        else if (p == "flag_before_each_value")
            ctx.addLine("cl::CommandFlagBeforeEachValue{},");
        else if (p == "config_variable")
            ctx.addLine("cl::ConfigVariable{},");
        else if (p == "separate_prefix")
            ctx.addLine("cl::SeparatePrefix{},");
        else
            throw SW_RUNTIME_ERROR("unknown property: " + p);
    }
    ctx.endBlock(true);
    ctx.emptyLines(1);
}

void Flag::printEnum(primitives::CppEmitter &ctx) const
{
    if (disabled)
        return;
    if (enum_vals.empty())
        return;

    if (!ns.empty())
        ctx.beginNamespace(ns);
    ctx.beginBlock("enum class " + type);
    for (auto &[e, ev] : enum_vals)
        ctx.addLine(e + ",");
    ctx.endBlock(true);
    ctx.emptyLines(1);
    if (!ns.empty())
        ctx.endNamespace(ns);
    ctx.emptyLines(1);
    ctx.addLine("DECLARE_OPTION_SPECIALIZATION(" + getTypeWithNs() + ");");
    ctx.emptyLines(1);
}

void Flag::printStruct(primitives::CppEmitter &ctx) const
{
    if (disabled)
        return;
    if (struct_.empty())
        return;

    if (!ns.empty())
        ctx.beginNamespace(ns);
    ctx.beginBlock("struct " + type);
    ctx.addLine(struct_);
    ctx.endBlock(true);
    ctx.emptyLines(1);
    if (!ns.empty())
        ctx.endNamespace(ns);
    ctx.emptyLines(1);
    ctx.addLine("DECLARE_OPTION_SPECIALIZATION(" + getTypeWithNs() + ");");
    ctx.emptyLines(1);
}

void Flag::printStructFunction(primitives::CppEmitter &ctx) const
{
    if (disabled)
        return;
    if (struct_.empty())
        return;
    if (function.empty())
        throw SW_RUNTIME_ERROR("Empty function for struct");

    ctx.beginFunction("DECLARE_OPTION_SPECIALIZATION(" + getTypeWithNs() + ")");
    ctx.addLine(function);
    ctx.endBlock();
    ctx.emptyLines(1);
}

void Flag::printToIde(primitives::CppEmitter &ctx) const
{
    if (disabled)
        return;
    if (!print_to_ide)
        return;

    if (!enum_vals.empty())
    {
        ctx.addLine("ctx.beginBlock(\"" + getIdeName() + "\");");
        ctx.beginBlock("switch (" + name + ".value())");
        for (auto &[e, ev] : enum_vals)
        {
            ctx.addLine("case ");
            if (!ns.empty())
                ctx.addText(ns + "::");
            if (!enum_vals.empty())
                ctx.addText(type + "::");
            ctx.addText(e + ":");
            ctx.increaseIndent();
            ctx.addLine("ctx.addText(\"" + ev.getIdeName() + "\");");
            ctx.addLine("break;");
            ctx.decreaseIndent();
        }
        ctx.endBlock();
        ctx.addLine("ctx.endBlock(true);");
        ctx.emptyLines(1);
        return;
    }

    if (default_ide_value.empty())
        ctx.beginBlock("if (" + name + ")");
    ctx.addLine("ctx.beginBlock(\"" + getIdeName() + "\");");
    if (!default_ide_value.empty())
        ctx.beginBlock("if (" + name + ")");
    if (type == "bool")
    {
        if (ide_value.empty())
            ctx.addLine("ctx.addText(" + name + ".value() ? \"true\" : \"false\");");
        else
            ctx.addLine("ctx.addText(" + name + ".value() ? \"" + ide_value + "\" : \"false\");");
    }
    else if (type == "path")
        ctx.addLine("ctx.addText(" + name + ".value().u8string());");
    else if (type == "String" || type == "std::string")
        ctx.addLine("ctx.addText(" + name + ".value().u8string());");
    else // numeric
        ctx.addLine("ctx.addText(std::to_string(" + name + ".value()));");
    if (!default_ide_value.empty())
    {
        ctx.endBlock();
        ctx.beginBlock("else");
        if (type == "bool")
            ctx.addLine("ctx.addText(" + default_ide_value + " ? \"true\" : \"false\");");
        else
            ctx.addLine("ctx.addText(" + default_ide_value + "");
        ctx.endBlock();
    }
    ctx.addLine("ctx.endBlock(true);");
    if (default_ide_value.empty())
        ctx.endBlock();
    ctx.emptyLines(1);
}

void Flag::printCommandLine(primitives::CppEmitter &ctx) const
{
    if (disabled)
        return;
    if (type.empty())
        return;

    if (0);
    else if (type == "bool")
    {
        ctx.increaseIndent("if (" + name + ")");
        ctx.addLine("s.push_back(\"-" + flag + "\");");
        ctx.decreaseIndent();
    }
    else if (type == "path")
    {
    }
    //else
    //throw SW_RUNTIME_ERROR("unknown cpp type: " + v.type);
}

std::vector<const Flag *> Type::sortFlags() const
{
    std::vector<const Flag*> flags2;
    for (auto &[k, v] : flags)
        flags2.push_back(&v);
    std::sort(flags2.begin(), flags2.end(), [](const auto &f1, const auto &f2)
        {
            return f1->order < f2->order;
        });
    return flags2;
}

void Type::print(primitives::CppEmitter &h, primitives::CppEmitter &cpp) const
{
    if (printed)
        return;

    printH(h);
    printCpp(cpp);

    printed = true;
}

void Type::printH(primitives::CppEmitter &h) const
{
    auto flags2 = sortFlags();

    // print enums and struct
    for (auto &v : flags2)
    {
        v->printEnum(h);
        v->printStruct(h);
    }

    // print command opts
    h.beginBlock("struct SW_DRIVER_CPP_API " + name + (parent.empty() ? "" : (" : " + parent)));
    for (auto &v : flags2)
        v->printDecl(h);
    h.emptyLines(1);

    h.addLine("Strings getCommandLine(const ::sw::builder::Command &c);");
    //h.addLine("void printIdeSettings(ProjectEmitter &);");

    h.endBlock(true);
    h.addLine("DECLARE_OPTION_SPECIALIZATION(" + name + ");");
    h.emptyLines(1);
}

void Type::printCpp(primitives::CppEmitter &cpp) const
{
    auto flags2 = sortFlags();

    cpp.addLine("DEFINE_OPTION_SPECIALIZATION_DUMMY(" + name + ")");
    cpp.addLine();

    cpp.beginBlock("Strings " + name + "::getCommandLine(const ::sw::builder::Command &c)");
    cpp.addLine("Strings s;");
    if (!parent.empty())
        cpp.addLine("s = " + parent + "::getCommandLine(c);");
    for (auto &v : flags2)
        v->printCommandLine(cpp);
    cpp.addLine("return s;");
    cpp.endBlock();
    cpp.emptyLines(1);

    /*cpp.beginBlock("void " + name + "::printIdeSettings(ProjectEmitter &ctx)");

    if (!parent.empty())
    {
        cpp.addLine(parent + "::printIdeSettings(ctx);");
        cpp.emptyLines(1);
    }

    for (auto &v : flags2)
        v->printToIde(cpp);
    cpp.endBlock();
    cpp.emptyLines(1);*/

    for (auto &v : flags2)
        v->printStructFunction(cpp);
    cpp.emptyLines(1);
}

void File::print_type(const Type &t, primitives::CppEmitter &h, primitives::CppEmitter &cpp) const
{
    if (!t.parent.empty())
    {
        for (const auto &[k, v] : types)
        {
            if (v.name == t.parent)
                print_type(v, h, cpp);
        }
    }
    t.print(h, cpp);
}

void File::print(primitives::CppEmitter &h, primitives::CppEmitter &cpp) const
{
    for (const auto &[k, v] : types)
        print_type(v, h, cpp);
}

void read_flags(const yaml &root, Flags &flags)
{
    get_map_and_iterate(root, "flags", [&flags](const auto &kv)
    {
        Flag fl;
        if (kv.second["name"].IsDefined())
            fl.name = kv.second["name"].template as<String>();
        else
            throw SW_RUNTIME_ERROR("missing name field");
        if (kv.second["ide_name"].IsDefined())
            fl.ide_name = kv.second["ide_name"].template as<String>();
        if (kv.second["flag"].IsDefined())
            fl.flag = kv.second["flag"].template as<String>();
        if (kv.second["namespace"].IsDefined())
            fl.ns = kv.second["namespace"].template as<String>();
        if (kv.second["type"].IsDefined())
            fl.type = kv.second["type"].template as<String>();
        if (kv.second["default"].IsDefined())
            fl.default_value = kv.second["default"].template as<String>();
        if (kv.second["default_ide_value"].IsDefined())
        {
            fl.default_ide_value = kv.second["default_ide_value"].template as<String>();
            fl.print_to_ide = true;
        }
        if (kv.second["ide_value"].IsDefined())
            fl.ide_value = kv.second["ide_value"].template as<String>();
        if (kv.second["enum"].IsDefined())
        {
            if (!kv.second["enum"].IsSequence())
                throw SW_RUNTIME_ERROR("enum must be a sequence");
            get_sequence_and_iterate(kv.second, "enum", [&fl](const auto &v)
            {
                if (v.IsScalar())
                {
                    auto u = v.template as<String>();
                    fl.enum_vals[u].name = u;
                }
                else if (v.IsMap())
                {
                    for (const auto &kv2 : v)
                    {
                        auto u = kv2.first.template as<String>();
                        fl.enum_vals[u].name = u;
                        fl.enum_vals[u].ide_name = kv2.second.template as<String>();
                    }
                }
                else
                    throw SW_RUNTIME_ERROR(fl.name + ": enum value must be a scalar or map");
            });
        }
        if (kv.second["order"].IsDefined())
            fl.order = kv.second["order"].template as<int>();
        if (kv.second["function"].IsDefined())
            fl.function = kv.second["function"].template as<String>();
        if (kv.second["function_current"].IsDefined())
            fl.function_current = kv.second["function_current"].template as<String>();
        if (kv.second["struct"].IsDefined())
            fl.struct_ = kv.second["struct"].template as<String>();
        if (kv.second["disabled"].IsDefined())
            fl.disabled = kv.second["disabled"].template as<bool>();
        get_sequence_and_iterate(kv.second, "properties", [&fl](const auto &kv)
        {
            auto s = kv.template as<String>();
            if (s == "print_to_ide")
                fl.print_to_ide = true;
            else
                fl.properties.insert(s);
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

    primitives::CppEmitter hctx, cctx;

    both(hctx, cctx, "// generated file, do not edit");
    both(hctx, cctx);

    hctx.addLine("#pragma once");
    hctx.addLine();
    hctx.beginNamespace("sw");
    cctx.beginNamespace("sw");

    f.print(hctx, cctx);

    hctx.endNamespace();
    cctx.endNamespace();

    write_file/*_if_different*/(h, hctx.getText());
    write_file/*_if_different*/(cpp, cctx.getText());

    return 0;
}
