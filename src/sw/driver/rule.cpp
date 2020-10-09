// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Egor Pugin <egor.pugin@gmail.com>

#include "rule.h"

#include "compiler/compiler.h"
#include "target/native.h"

#include <primitives/exceptions.h>

namespace sw
{

IRule::~IRule() = default;

NativeRule::NativeRule(RuleProgram p)
    : program(p)
{
}

NativeCompilerRule::NativeCompilerRule(RuleProgram p, const StringSet &exts)
    : NativeRule(p), exts(exts)
{
}

NativeCompiler &NativeCompilerRule::getCompiler() const
{
    return static_cast<NativeCompiler &>(program);
}

Commands NativeRule::getCommands() const
{
    Commands cmds;
    for (auto &c : commands)
        cmds.insert(c->getCommand());
    return cmds;
}

static path getObjectFilename(const Target &t, const path &p)
{
    // target may push its files to outer packages,
    // so files must be concatenated with its target name
    // ^^^ wrong?
    // target push files, they'll use local definitions etc.
    return to_string(p.filename().u8string()) + "." + sha256(
        //t.pkg.toString() +
        to_string(p.u8string())).substr(0, 8);
}

static path getOutputFile(const Target &t, const path &input)
{
    auto o = t.BinaryDir.parent_path() / "obj" / getObjectFilename(t, input);
    o = fs::absolute(o);
    return o;
}

template <class C>
static path getOutputFile(const Target &t, const C &c, const path &input)
{
    return getOutputFile(t, input) += c.getObjectExtension(t.getBuildSettings().TargetOS);
}

Files NativeCompilerRule::addInputs(const Target &t, const RuleFiles &rfs)
{
    Files outputs;
    for (auto &rf : rfs)
    {
        if (!exts.contains(rf.getFile().extension().string()))
            continue;
        if (used_files.contains(rf))
            continue;
        auto output = getOutputFile(t, getCompiler(), rf.getFile());
        outputs.insert(output);
        auto c = getCompiler().clone();
        static_cast<NativeCompiler &>(*c).setSourceFile(rf.getFile(), output);
        static_cast<NativeCompiler &>(*c).prepareCommand(t);
        commands.emplace_back(std::move(c));
        used_files.insert(rf);
    }
    return outputs;
}

NativeLinker &NativeLinkerRule::getLinker() const
{
    return static_cast<NativeLinker &>(program);
}

Files NativeLinkerRule::addInputs(const Target &t, const RuleFiles &rfs)
{
    FilesOrdered files;
    for (auto &rf : rfs)
    {
        if (1
            && rf.getFile().extension() != ".obj"
            && rf.getFile().extension() != ".lib"
            && rf.getFile().extension() != ".res"
            )
            continue;
        if (used_files.contains(rf))
            continue;
        files.push_back(rf.getFile());
        used_files.insert(rf);
    }
    std::sort(files.begin(), files.end());
    if (files.empty())
        return {};

    Files outputs;

    auto c = getLinker().clone();
    static_cast<NativeLinker &>(*c).setObjectFiles(files);
    static_cast<NativeLinker &>(*c).prepareCommand(t);
    outputs.insert(static_cast<NativeLinker &>(*c).getOutputFile());
    commands.emplace_back(std::move(c));

    return outputs;
}

Files RcRule::addInputs(const Target &t, const RuleFiles &rfs)
{
    Files outputs;
    for (auto &rf : rfs)
    {
        if (rf.getFile().extension() != ".rc")
            continue;
        if (used_files.contains(rf))
            continue;
        auto output = getOutputFile(t, rf.getFile()) += ".res";
        outputs.insert(output);
        auto c = program.clone();
        static_cast<RcTool &>(*c).setSourceFile(rf.getFile());
        static_cast<RcTool &>(*c).setOutputFile(output);
        static_cast<RcTool &>(*c).prepareCommand(t);
        commands.emplace_back(std::move(c));
        used_files.insert(rf);
    }
    return outputs;
}

}