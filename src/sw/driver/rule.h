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

struct IRule : ICastable
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

struct NativeRule : IRule
{
    ProgramPtr program;

    NativeRule(ProgramPtr);

    virtual void setup(const Target &t) = 0;
    virtual Files addInputs(const Target &t, const RuleFiles &) = 0;

    Commands getCommands() const override;

protected:
    std::vector<ProgramPtr> commands;
    RuleFiles used_files;
};

struct NativeCompilerRule : NativeRule
{
    StringSet exts;

    NativeCompilerRule(ProgramPtr, const StringSet &exts);

    void setup(const Target &t) override;
    Files addInputs(const Target &t, const RuleFiles &) override;

private:
    NativeCompiler &getCompiler() const;
};

struct NativeLinkerRule : NativeRule
{
    using NativeRule::NativeRule;

    void setup(const Target &t) override;
    Files addInputs(const Target &t, const RuleFiles &) override;

    void setOutputFile(const path &);

private:
    NativeLinker &getLinker() const;
};

} // namespace sw
