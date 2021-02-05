// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "program.h"
#include "rule_file.h"

#include <sw/builder/command.h>
#include <sw/core/settings.h>
#include <sw/core/target.h>
#include <sw/support/package.h>

namespace sw
{

struct NativeLinker;
struct NativeCompiler;
struct NativeCompiledTarget;
struct Target;

struct SW_DRIVER_CPP_API IRule : ICastable
{
    //using RuleFiles = std::set<RuleFile>;
    //RuleFiles files;

    virtual ~IRule() = 0;

    // get commands for ... (building?)
    ///
    //virtual Commands getCommands() const = 0;

    /// add inputs to rule
    /// returns outputs
    //virtual Files addInputs(const RuleFiles &) = 0;

    virtual void setup(const Target &t) {}

    virtual std::unique_ptr<IRule> clone() const = 0;
};

using IRulePtr = std::unique_ptr<IRule>;

struct SW_DRIVER_CPP_API NativeRule : IRule
{
    using RuleProgram = ProgramPtr;

    decltype(builder::Command::arguments) arguments; // move to rule promise?

    NativeRule(RuleProgram);
    NativeRule(const NativeRule &rhs)
    {
        if (rhs.program)
            program = rhs.program->clone();
        arguments = rhs.arguments;
    }

    virtual void addInputs(const Target &t, RuleFiles &rfs);
    virtual RuleFiles addInput(const Target &t, const RuleFiles &, RuleFile &) { SW_UNREACHABLE; }

protected:
    RuleProgram program;

    Program &getProgram() const { return *program; }
    static path getOutputFileBase(const Target &t, const path &input);
    static path getOutputFile(const Target &t, const path &input);
};

struct SW_DRIVER_CPP_API NativeCompilerRule : NativeRule
{
    enum
    {
        LANG_ASM,
        LANG_C,
        LANG_CPP,
    };

    StringSet exts;
    //String rulename; // this can be now set ourselves?
    int lang = LANG_CPP;

    using NativeRule::NativeRule;

    IRulePtr clone() const override { return std::make_unique<NativeCompilerRule>(*this); }
    void addInputs(const Target &t, RuleFiles &) override;
    void setup(const Target &t) override;

    bool isC() const { return lang == LANG_C; }
    bool isCpp() const { return lang == LANG_CPP; }
    bool isAsm() const { return lang == LANG_ASM; }

    //Commands getCommands() const override { return commands; }

protected:
    //Commands commands;
};

struct SW_DRIVER_CPP_API NativeLinkerRule : NativeRule
{
    // librarian otherwise
    bool is_linker = true;

    using NativeRule::NativeRule;

    IRulePtr clone() const override { return std::make_unique<NativeLinkerRule>(*this); }
    void addInputs(const Target &t, RuleFiles &) override;
    void setup(const Target &t) override;

    /*Commands getCommands() const override
    {
        Commands commands;
        if (command)
            commands.insert(command);
        if (command_lib)
            commands.insert(command_lib);
        return commands;
    }*/

protected:
    //std::shared_ptr<builder::Command> command;
    //std::shared_ptr<builder::Command> command_lib;
};

struct RcRule : NativeRule
{
    using NativeRule::NativeRule;

    IRulePtr clone() const override { return std::make_unique<RcRule>(*this); }
    RuleFiles addInput(const Target &, const RuleFiles &, RuleFile &) override;
    void setup(const Target &) override;

    //Commands getCommands() const override { return commands; }

protected:
    //Commands commands;
};

} // namespace sw
