// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "options_cl_vs.h"

#include "command.h"

namespace sw
{

DEFINE_OPTION_SPECIALIZATION_DUMMY(VisualStudioAssemblerOptions);
DEFINE_OPTION_SPECIALIZATION_DUMMY(VisualStudioCompilerOptions);
DEFINE_OPTION_SPECIALIZATION_DUMMY(VisualStudioLinkerOptions);
DEFINE_OPTION_SPECIALIZATION_DUMMY(CPPLanguageStandard);

DEFINE_OPTION_SPECIALIZATION_DUMMY(VisualStudioCsCompilerOptions);

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
            o += create->string();
            //c->addOutput(create.value());
        }
        s.push_back(o);
    }
    if (use)
    {
        String o = "-Yu";
        if (!use->empty())
        {
            o += use->string();
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
    case MachineType::X86:
        s += "X86";
        break;
    case MachineType::X64:
        s += "X64";
        break;
    case MachineType::ARM:
        s += "ARM";
        break;
    case MachineType::ARM64:
        s += "ARM64";
        break;
    case MachineType::EBC:
        s += "EBC";
        break;
    default:
        throw SW_RUNTIME_EXCEPTION("unreachable code");
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
        throw SW_RUNTIME_EXCEPTION("unreachable code");
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
        throw SW_RUNTIME_EXCEPTION("unreachable code");
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
        throw SW_RUNTIME_EXCEPTION("unreachable code");
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
        throw SW_RUNTIME_EXCEPTION("unreachable code");
    }
    return { s };
}

DECLARE_OPTION_SPECIALIZATION(vs::PrecompiledHeaderVs)
{
    return value().getCommandLine(c);
}

DECLARE_OPTION_SPECIALIZATION(vs::Warnings)
{
    using namespace vs;

    auto &w = value();

    Strings s;
    if (w.DisableAll)
        s.push_back("-w");
    if (w.EnableAll)
        s.push_back("-Wall");
    if (w.Level >= 0 && w.Level <= 4)
        s.push_back("-W" + std::to_string(w.Level));
    for (auto &d : w.Disable)
        s.push_back("-wd" + std::to_string(d));
    for (auto &[l, ws] : w.DisableOnLevel)
    {
        for (auto &d : ws)
            s.push_back("-wd" + std::to_string(l) + std::to_string(d));
    }
    if (w.TreatAllWarningsAsError)
        s.push_back("-WX");
    for (auto &d : w.TreatAsError)
        s.push_back("-we" + std::to_string(d));
    for (auto &d : w.DisplayOnce)
        s.push_back("-wo" + std::to_string(d));
    if (w.EnableOneLineDiagnostics)
        s.push_back("-WL");

    return { s };
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
    using namespace vs;

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
        throw SW_RUNTIME_EXCEPTION("unreachable code");
    }
    return { s };
}

}
