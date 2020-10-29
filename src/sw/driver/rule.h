// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "program.h"

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

struct RuleFile
{
    using AdditionalArguments = std::vector<String>;

    RuleFile(const path &fn) : file(fn) {}

    AdditionalArguments &getAdditionalArguments() { return additional_arguments; }
    const AdditionalArguments &getAdditionalArguments() const { return additional_arguments; }

    bool operator<(const RuleFile &rhs) const { return std::tie(file, additional_arguments) < std::tie(rhs.file, rhs.additional_arguments); }
    bool operator==(const RuleFile &rhs) const { return std::tie(file, additional_arguments) == std::tie(rhs.file, rhs.additional_arguments); }
    //auto operator<=>(const RuleFile &rhs) const = default;

    const path &getFile() const { return file; }

private:
    path file;
    AdditionalArguments additional_arguments;
};

using RuleFiles = std::set<RuleFile>;

struct SW_DRIVER_CPP_API NativeRule : IRule
{
    using RuleProgram = Program &;

    RuleProgram program;
    decltype(builder::Command::arguments) arguments; // move to rule promise?

    NativeRule(RuleProgram);
    NativeRule(const NativeRule &rhs) : program(rhs.program) { arguments = rhs.arguments; }

    virtual Files addInputs(const Target &t, const RuleFiles &) = 0;
    virtual void setup(const Target &t) {}

    Commands getCommands() const override;

protected:
    std::vector<ProgramPtr> commands;
    RuleFiles used_files;
    Commands commands2;
};

struct SW_DRIVER_CPP_API NativeCompilerRule : NativeRule
{
    StringSet exts;
    //String rulename;

    NativeCompilerRule(RuleProgram, const StringSet &exts);

    IRulePtr clone() const override { return std::make_unique<NativeCompilerRule>(*this); }
    Files addInputs(const Target &t, const RuleFiles &) override;
    void setup(const Target &t) override;

private:
    NativeCompiler &getCompiler() const;
};

struct SW_DRIVER_CPP_API NativeLinkerRule : NativeRule
{
    // librarian otherwise
    bool is_linker = true;

    NativeLinkerRule(ProgramPtr);
    NativeLinkerRule(const NativeLinkerRule &rhs)
        : NativeRule(rhs)
    {
        if (rhs.p)
            p = rhs.p->clone();
    }

    IRulePtr clone() const override { return std::make_unique<NativeLinkerRule>(*this); }
    Files addInputs(const Target &t, const RuleFiles &) override;
    void setup(const Target &t) override;

private:
    ProgramPtr p;
};

struct RcRule : NativeRule
{
    RcRule(ProgramPtr);
    RcRule(const RcRule &rhs)
        : NativeRule(rhs)
    {
        if (rhs.p)
            p = rhs.p->clone();
    }

    IRulePtr clone() const override { return std::make_unique<RcRule>(*this); }
    Files addInputs(const Target &t, const RuleFiles &) override;
    void setup(const Target &t) override;

private:
    ProgramPtr p;
};

} // namespace sw
