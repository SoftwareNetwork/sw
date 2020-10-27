// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "program.h"

#include <sw/builder/command.h>
#include <sw/core/settings.h>
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

struct SW_DRIVER_CPP_API IRule : ICastable
{
    using RuleFiles = std::set<RuleFile>;

    RuleFiles files;

    virtual ~IRule() = 0;

    // get commands for ... (building?)
    ///
    virtual Commands getCommands() const = 0;

    /// add inputs to rule
    /// returns outputs
    //virtual Files addInputs(const RuleFiles &) = 0;
};

struct SW_DRIVER_CPP_API NativeRule : IRule
{
    using RuleProgram = Program &;

    RuleProgram program;
    decltype(builder::Command::arguments) arguments; // move to rule promise?

    NativeRule(RuleProgram);
    NativeRule(const NativeRule &) = delete;

    virtual Files addInputs(Target &t, const RuleFiles &) = 0;
    virtual void setup(const Target &t) {}

    Commands getCommands() const override;

protected:
    std::vector<ProgramPtr> commands;
    Commands commands2;
    RuleFiles used_files;
};

struct SW_DRIVER_CPP_API NativeCompilerRule : NativeRule
{
    StringSet exts;
    //String rulename;

    NativeCompilerRule(RuleProgram, const StringSet &exts);

    Files addInputs(Target &t, const RuleFiles &) override;
    void setup(const Target &t) override;

private:
    NativeCompiler &getCompiler() const;
};

struct SW_DRIVER_CPP_API NativeLinkerRule : NativeRule
{
    using NativeRule::NativeRule;

    Files addInputs(Target &t, const RuleFiles &) override;
    void setup(const Target &t) override;

private:
    NativeLinker &getLinker() const;
};

struct RcRule : NativeRule
{
    RcRule(ProgramPtr);

    Files addInputs(Target &t, const RuleFiles &) override;
    void setup(const Target &t) override;

private:
    ProgramPtr p;
};

} // namespace sw
