/*
 * SW - Build System and Package Manager
 * Copyright (C) 2017-2020 Egor Pugin
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "options_cl_vs.h"

#include "command.h"

#include "options_cl.generated.cpp"

namespace sw
{

DEFINE_OPTION_SPECIALIZATION_DUMMY(CLanguageStandard);
DEFINE_OPTION_SPECIALIZATION_DUMMY(CPPLanguageStandard);

namespace vs
{

String ExceptionHandling::getCommandLine() const
{
    String s = "-EH";
    if (SEH)
        s += "a";
    else if (CPP)
        s += "s";
    if (!ExternCMayThrow)
        s += "c";
    if (TerminationChecks)
        s += "r";
    if (ClearFlag)
        s += "-";
    return s;
}

Strings PrecompiledHeaderVs::getCommandLine(builder::Command *c) const
{
    Strings s;
    if (create)
    {
        String o = "-Yc";
        if (!create->empty())
        {
            o += normalize_path(*create);
            //c->addOutput(create.value());
        }
        s.push_back(o);
    }
    if (use)
    {
        String o = "-Yu";
        if (!use->empty())
        {
            o += normalize_path(*use);
            //c->addInput(use.value());
        }
        // TODO: add autocreated name by cl.exe or whatever
        s.push_back(o);
    }
    if (with_debug_info)
        s.push_back("-Yd");
    if (ignore)
        s.push_back("-Y-");
    return s;
}

}

DECLARE_OPTION_SPECIALIZATION(vs::ExceptionHandlingVector)
{
    Strings s;
    for (auto &e : value())
        s.push_back(e.getCommandLine());
    return s;
}

DECLARE_OPTION_SPECIALIZATION(vs::MachineType)
{
    using namespace vs;

    auto s = getCommandLineFlag();
    switch (value())
    {
#define CASE(x)          \
    case MachineType::x: \
        s += #x;         \
        break

    CASE(X64);
    CASE(X86);

    CASE(ARM64);
    CASE(ARM);

    CASE(IA64);

    CASE(MIPS);
    CASE(MIPS16);
    CASE(MIPSFPU);
    CASE(MIPSFPU16);

    CASE(SH4);
    CASE(THUMB);
    CASE(EBC);

#undef CASE

    default:
        throw SW_RUNTIME_ERROR("unreachable code");
    }
    return { s };
}

DECLARE_OPTION_SPECIALIZATION(vs::RuntimeLibraryType)
{
    using namespace vs;

    auto s = "-"s;
    switch (value())
    {
    case RuntimeLibraryType::MultiThreaded:
        s += "MT";
        break;
    case RuntimeLibraryType::MultiThreadedDebug:
        s += "MTd";
        break;
    case RuntimeLibraryType::MultiThreadedDLL:
        s += "MD";
        break;
    case RuntimeLibraryType::MultiThreadedDLLDebug:
        s += "MDd";
        break;
    default:
        throw SW_RUNTIME_ERROR("unreachable code");
    }
    return { s };
}

DECLARE_OPTION_SPECIALIZATION(vs::DebugInformationFormatType)
{
    using namespace vs;

    auto s = "-"s;
    switch (value())
    {
    case DebugInformationFormatType::None:
        return {};
    case DebugInformationFormatType::ObjectFile:
        s += "Z7";
        break;
    case DebugInformationFormatType::ProgramDatabase:
        s += "Zi";
        break;
    case DebugInformationFormatType::ProgramDatabaseEditAndContinue:
        s += "ZI";
        break;
    default:
        throw SW_RUNTIME_ERROR("unreachable code");
    }
    return { s };
}

DECLARE_OPTION_SPECIALIZATION(vs::Subsystem)
{
    using namespace vs;

    auto s = getCommandLineFlag();
    switch (value())
    {
    case Subsystem::Console:
        s += "CONSOLE";
        break;
    case Subsystem::Windows:
        s += "WINDOWS";
        break;
    default:
        throw SW_RUNTIME_ERROR("unreachable code");
    }
    return { s };
}

DECLARE_OPTION_SPECIALIZATION(vs::link::Debug)
{
    Strings s;
    switch (value())
    {
    case vs::link::Debug::None:
        s.push_back(getCommandLineFlag() + "NONE");
        break;
    case vs::link::Debug::FastLink:
        s.push_back(getCommandLineFlag() + "FASTLINK");
        break;
    case vs::link::Debug::Full:
        s.push_back(getCommandLineFlag() + "FULL");
        break;
    }
    return { s };
}

DECLARE_OPTION_SPECIALIZATION(vs::ForceType)
{
    using namespace vs;

    auto s = getCommandLineFlag();
    switch (value())
    {
    case ForceType::Multiple:
        s += "MULTIPLE";
        break;
    case ForceType::Unresolved:
        s += "UNRESOLVED";
        break;
    default:
        throw SW_RUNTIME_ERROR("unreachable code");
    }
    return { s };
}

DECLARE_OPTION_SPECIALIZATION(vs::PrecompiledHeaderVs)
{
    return value().getCommandLine(c);
}

DECLARE_OPTION_SPECIALIZATION(vs::Optimizations)
{
    using namespace vs;

    auto &o = value();

    Strings s;
    if (!o.Disable)
    {
        if (o.Level == 1 || o.SmallCode)
            s.push_back("-O1");
        else if (o.Level == 2 || o.FastCode)
            s.push_back("-O2");
    }
    if (o.Disable)
        s.push_back("-Od");

    return { s };
}

Strings getCommandLineImplCPPLanguageStandardVS(const CommandLineOption<CPPLanguageStandard> &co, builder::Command *c)
{
    String s = "-std:c++";
    switch (co.value())
    {
    case CPPLanguageStandard::CPP14:
        s += "14";
        break;
    case CPPLanguageStandard::CPP17:
        s += "17";
        break;
    case CPPLanguageStandard::CPPLatest:
        s += "latest";
        break;
    default:
        return {};
    }
    return { s };
}

DECLARE_OPTION_SPECIALIZATION(vs::cs::Target)
{
    auto s = getCommandLineFlag();
    switch (value())
    {
    case vs::cs::Target::Console:
        s += "exe";
        break;
    case vs::cs::Target::Windows:
        s += "winexe";
        break;
    case vs::cs::Target::Library:
        s += "library";
        break;
    case vs::cs::Target::Module:
        s += "module";
        break;
    case vs::cs::Target::AppContainer:
        s += "appcontainerexe";
        break;
    case vs::cs::Target::Winmdobj:
        s += "winmdobj";
        break;
    default:
        throw SW_RUNTIME_ERROR("unreachable code");
    }
    return { s };
}

DECLARE_OPTION_SPECIALIZATION(rust::CrateType)
{
    String s;
    switch (value())
    {
#define CASE(x)              \
    case rust::CrateType::x: \
        s += #x;             \
        break

        CASE(bin);
        CASE(lib);
        CASE(rlib);
        CASE(dylib);
        CASE(cdylib);
        CASE(staticlib);

#undef CASE

    case rust::CrateType::proc_macro:
        s += "proc-macro";
        break;
    default:
        throw SW_RUNTIME_ERROR("unreachable code");
    }
    return { getCommandLineFlag(), s };
}

DECLARE_OPTION_SPECIALIZATION(clang::ArchType)
{
    Strings s;
    switch (value())
    {
    case clang::ArchType::m32:
        s.push_back("-m32");
        break;
    case clang::ArchType::m64:
        s.push_back("-m64");
        break;
    default:
        SW_UNIMPLEMENTED;
    }
    return { s };
}

DECLARE_OPTION_SPECIALIZATION(gnu::Optimizations)
{
    auto &o = value();

    Strings s;
    if (!o.Disable)
    {
        if (o.Level)
            s.push_back("-O" + std::to_string(o.Level.value()));
        if (o.FastCode)
            s.push_back("-Ofast");
        if (o.SmallCode)
            s.push_back("-Os");
    }
    //if (o.Disable)
        //s.push_back("-Od");

    return { s };
}

}
