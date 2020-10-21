// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#include "other.h"

#include "common.h"

#include "sw/driver/build.h"
#include "sw/driver/compiler/detect.h"

#include <sw/core/sw_context.h>
#include <sw/manager/storage.h>

namespace sw
{

void ProgramDetector::detectAdaCompilers(DETECT_ARGS)
{
    auto p = std::make_shared<SimpleProgram>();
    auto f = resolveExecutable("gnatmake");
    if (!fs::exists(f))
        return;
    p->file = f;

    auto v = getVersion(s, p->file, "--version", "(\\d{4})(\\d{2})(\\d{2})");
    addProgram(DETECT_ARGS_PASS, PackageId("org.gnu.gcc.ada", v), {}, p);
}

AdaTarget::AdaTarget(TargetBase &parent, const PackageId &id)
    : Target(parent, id), NativeTargetOptionsGroup((Target &)*this)
{
}

bool AdaTarget::init()
{
    //static std::once_flag f;
    //std::call_once(f, [this] {detectAdaCompilers(DETECT_ARGS_PASS_FIRST_CALL_SIMPLE); });

    Target::init();

    compiler = activateCompiler<AdaCompiler>(*this, "org.gnu.gcc.ada"s, { ".adb", ".ads" });
    if (!compiler)
        throw SW_RUNTIME_ERROR("No Ada compiler found");

    compiler->Extension = getBuildSettings().TargetOS.getExecutableExtension();
    compiler->setOutputFile(getBaseOutputFileName(*this, {}, "bin"));

    SW_RETURN_MULTIPASS_END(init_pass);
}

Commands AdaTarget::getCommands1() const
{
    // https://gcc.gnu.org/onlinedocs/gcc-10.1.0/gnat_ugn.pdf
    // gnat compile hello.adb
    // gnat bind -x hello.ali
    // gnat link hello.ali

    // how to change output file?
    // works:
    // gnatmake -o ... input.adb

    for (auto f : gatherSourceFiles<SourceFile>(*this, { ".adb", ".ads" }))
        compiler->addSourceFile(f->file);
    Commands cmds;
    auto c = compiler->getCommand(*this);
    c->working_directory = getObjectDir();
    cmds.insert(c);
    return cmds;
}

void ProgramDetector::detectCSharpCompilers(DETECT_ARGS)
{
    for (auto &[v, i] : vsinstances)
    {
        auto root = i.root;
        switch (v.getMajor())
        {
        case 15:
            root = root / "MSBuild" / "15.0" / "Bin" / "Roslyn";
            break;
        case 16:
            root = root / "MSBuild" / "Current" / "Bin" / "Roslyn";
            break;
        default:
            SW_UNIMPLEMENTED;
        }

        auto p = std::make_shared<SimpleProgram>();
        p->file = root / "csc.exe";

        auto v1 = getVersion(s, p->file);
        addProgram(DETECT_ARGS_PASS, PackageId("com.Microsoft.VisualStudio.Roslyn.csc", v1), {}, p);
    }
}

CSharpTarget::CSharpTarget(TargetBase &parent, const PackageId &id)
    : Target(parent, id), NativeTargetOptionsGroup((Target &)*this)
{
}

bool CSharpTarget::init()
{
    //static std::once_flag f;
    //std::call_once(f, [this] {detectCSharpCompilers(DETECT_ARGS_PASS_FIRST_CALL_SIMPLE); });

    Target::init();

    compiler = activateCompiler<VisualStudioCSharpCompiler>(*this, "com.Microsoft.VisualStudio.Roslyn.csc"s, { ".cs" });
    if (!compiler)
        throw SW_RUNTIME_ERROR("No C# compiler found");

    compiler->Extension = getBuildSettings().TargetOS.getExecutableExtension();
    compiler->setOutputFile(getBaseOutputFileName(*this, {}, "bin"));

    SW_RETURN_MULTIPASS_END(init_pass);
}

Commands CSharpTarget::getCommands1() const
{
    for (auto f : gatherSourceFiles<SourceFile>(*this, { ".cs" }))
        compiler->addSourceFile(f->file);

    Commands cmds;
    auto c = compiler->getCommand(*this);
    cmds.insert(c);
    return cmds;
}

void ProgramDetector::detectRustCompilers(DETECT_ARGS)
{
    auto p = std::make_shared<SimpleProgram>();
    auto f = resolveExecutable("rustc");
    if (!fs::exists(f))
    {
        f = resolveExecutable(get_home_directory() / ".cargo" / "bin" / "rustc");
        if (!fs::exists(f))
            return;
    }
    p->file = f;

    auto v = getVersion(s, p->file);
    addProgram(DETECT_ARGS_PASS, PackageId("org.rust.rustc", v), {}, p);
}

RustTarget::RustTarget(TargetBase &parent, const PackageId &id)
    : Target(parent, id), NativeTargetOptionsGroup((Target &)*this)
{
}

bool RustTarget::init()
{
    //static std::once_flag f;
    //std::call_once(f, [this] {detectRustCompilers(DETECT_ARGS_PASS_FIRST_CALL_SIMPLE); });

    Target::init();

    compiler = activateCompiler<decltype(compiler)::element_type>(*this, "org.rust.rustc"s, { ".rs" });
    if (!compiler)
        throw SW_RUNTIME_ERROR("No Rust compiler found");

    compiler->Extension = getBuildSettings().TargetOS.getExecutableExtension();
    compiler->setOutputFile(getBaseOutputFileName(*this, {}, "bin"));

    SW_RETURN_MULTIPASS_END(init_pass);
}

Commands RustTarget::getCommands1() const
{
    for (auto f : gatherSourceFiles<SourceFile>(*this, {".rs"}))
        compiler->setSourceFile(f->file);

    Commands cmds;
    auto c = compiler->getCommand(*this);
    cmds.insert(c);
    return cmds;
}

void ProgramDetector::detectGoCompilers(DETECT_ARGS)
{
    auto p = std::make_shared<SimpleProgram>();
    auto f = resolveExecutable("go");
    if (!fs::exists(f))
        return;
    p->file = f;

    auto v = getVersion(s, p->file, "version");
    addProgram(DETECT_ARGS_PASS, PackageId("org.google.golang.go", v), {}, p);
}

GoTarget::GoTarget(TargetBase &parent, const PackageId &id)
    : Target(parent, id), NativeTargetOptionsGroup((Target &)*this)
{
}

bool GoTarget::init()
{
    //static std::once_flag f;
    //std::call_once(f, [this] {detectGoCompilers(DETECT_ARGS_PASS_FIRST_CALL_SIMPLE); });

    Target::init();

    compiler = activateCompiler<decltype(compiler)::element_type>(*this, "org.google.golang.go"s, { ".go" });
    if (!compiler)
        throw SW_RUNTIME_ERROR("No Go compiler found");

    compiler->Extension = getBuildSettings().TargetOS.getExecutableExtension();
    compiler->setOutputFile(getBaseOutputFileName(*this, {}, "bin"));

    SW_RETURN_MULTIPASS_END(init_pass);
}

Commands GoTarget::getCommands1() const
{
    for (auto f : gatherSourceFiles<SourceFile>(*this, {".go"}))
        compiler->setSourceFile(f->file);

    Commands cmds;
    auto c = compiler->getCommand(*this);
    cmds.insert(c);
    return cmds;
}

void ProgramDetector::detectJavaCompilers(DETECT_ARGS)
{
    //compiler = resolveExecutable("jar"); // later

    auto p = std::make_shared<SimpleProgram>();
    auto f = resolveExecutable("javac");
    if (!fs::exists(f))
        return;
    p->file = f;

    auto v = getVersion(s, p->file);
    addProgram(DETECT_ARGS_PASS, PackageId("com.oracle.java.javac", v), {}, p);
}

JavaTarget::JavaTarget(TargetBase &parent, const PackageId &id)
    : Target(parent, id), NativeTargetOptionsGroup((Target &)*this)
{
}

bool JavaTarget::init()
{
    //static std::once_flag f;
    //std::call_once(f, [this] {detectJavaCompilers(DETECT_ARGS_PASS_FIRST_CALL_SIMPLE); });

    Target::init();

    compiler = activateCompiler<decltype(compiler)::element_type>(*this, "com.oracle.java.javac"s, { ".java" });
    if (!compiler)
        throw SW_RUNTIME_ERROR("No Java compiler found");

    compiler->setOutputDir(getBaseOutputDirName(*this, {}, "bin"));

    SW_RETURN_MULTIPASS_END(init_pass);
}

Commands JavaTarget::getCommands1() const
{
    Commands cmds;
    for (auto f : gatherSourceFiles<SourceFile>(*this, {".java"}))
    {
        compiler->setSourceFile(f->file);
    }

    auto c = compiler->getCommand(*this);
    cmds.insert(c);
    return cmds;
}

void ProgramDetector::detectKotlinCompilers(DETECT_ARGS)
{
    auto p = std::make_shared<SimpleProgram>();
    auto f = resolveExecutable("kotlinc");
    if (!fs::exists(f))
        return;
    p->file = f;

    auto v = getVersion(s, p->file, "-version");
    addProgram(DETECT_ARGS_PASS, PackageId("com.JetBrains.kotlin.kotlinc", v), {}, p);
}

KotlinTarget::KotlinTarget(TargetBase &parent, const PackageId &id)
    : Target(parent, id), NativeTargetOptionsGroup((Target &)*this)
{
}

bool KotlinTarget::init()
{
    //static std::once_flag f;
    //std::call_once(f, [this] {detectKotlinCompilers(DETECT_ARGS_PASS_FIRST_CALL_SIMPLE); });

    Target::init();

    compiler = activateCompiler<decltype(compiler)::element_type>(*this, "com.JetBrains.kotlin.kotlinc"s, { ".kt", ".kts" });
    if (!compiler)
        throw SW_RUNTIME_ERROR("No Kotlin compiler found");

    compiler->setOutputFile(getBaseOutputFileName(*this, {}, "bin"));

    SW_RETURN_MULTIPASS_END(init_pass);
}

Commands KotlinTarget::getCommands1() const
{
    for (auto f : gatherSourceFiles<SourceFile>(*this, { ".kt", ".kts" }))
        compiler->setSourceFile(f->file);

    Commands cmds;
    auto c = compiler->getCommand(*this);
    cmds.insert(c);
    return cmds;
}

NativeLinker *DTarget::getSelectedTool() const
{
    return compiler.get();
}

void ProgramDetector::detectDCompilers(DETECT_ARGS)
{
    // also todo LDC, GDC compiler

    auto p = std::make_shared<SimpleProgram>();
    auto f = resolveExecutable("dmd");
    if (!fs::exists(f))
        return;
    p->file = f;

    auto v = getVersion(s, p->file);
    addProgram(DETECT_ARGS_PASS, PackageId("org.dlang.dmd.dmd", v), {}, p);
}

DTarget::DTarget(TargetBase &parent, const PackageId &id)
    : NativeTarget(parent, id), NativeTargetOptionsGroup((Target &)*this)
{
}

bool DTarget::init()
{
    //static std::once_flag f;
    //std::call_once(f, [this] {detectDCompilers(DETECT_ARGS_PASS_FIRST_CALL_SIMPLE); });

    // https://dlang.org/dmd-windows.html
    // https://wiki.dlang.org/Win32_DLLs_in_D
    switch (init_pass)
    {
    case 1:
    {
        Target::init();

        compiler = activateCompiler<decltype(compiler)::element_type>(*this, "org.dlang.dmd.dmd"s, { ".d", /*.di*/ });
        if (!compiler)
            throw SW_RUNTIME_ERROR("No D compiler found");

        compiler->setObjectDir(BinaryDir.parent_path() / "obj");
    }
    SW_RETURN_MULTIPASS_NEXT_PASS(init_pass);
    case 2:
    {
        setOutputFile();
    }
    SW_RETURN_MULTIPASS_END(init_pass);
    }
    SW_RETURN_MULTIPASS_END(init_pass);
}

Commands DTarget::getCommands1() const
{
    for (auto f : gatherSourceFiles<SourceFile>(*this, {".d"}))
        compiler->setSourceFile(f->file/*, BinaryDir.parent_path() / "obj" / f->file.filename()*/);

    // add prepare() to propagate deps
    // here we check only our deps
    for (auto &d : this->gatherDependencies())
        compiler->setSourceFile(d->getTarget().as<DTarget &>().compiler->getOutputFile());

    Commands cmds;
    auto c = compiler->getCommand(*this);
    cmds.insert(c);
    return cmds;
}

bool DStaticLibrary::init()
{
    auto r = DTarget::init();
    compiler->Extension = getBuildSettings().TargetOS.getStaticLibraryExtension();
    compiler->BuildLibrary = true;
    return r;
}

bool DSharedLibrary::init()
{
    auto r = DTarget::init();
    compiler->Extension = getBuildSettings().TargetOS.getSharedLibraryExtension();
    compiler->BuildDll = true;
    return r;
}

bool DExecutable::init()
{
    auto r = DTarget::init();
    compiler->Extension = getBuildSettings().TargetOS.getExecutableExtension();
    return r;
}

void ProgramDetector::detectPascalCompilers(DETECT_ARGS)
{
    // free pascal for now

    auto p = std::make_shared<SimpleProgram>();
    auto f = resolveExecutable("fpc");
    if (!fs::exists(f))
        return;
    p->file = f;

    auto v = getVersion(s, p->file, "-version");
    addProgram(DETECT_ARGS_PASS, PackageId("org.pascal.fpc", v), {}, p);
}

PascalTarget::PascalTarget(TargetBase &parent, const PackageId &id)
    : Target(parent, id), NativeTargetOptionsGroup((Target &)*this)
{
}

bool PascalTarget::init()
{
    //static std::once_flag f;
    //std::call_once(f, [this] {detectPascalCompilers(DETECT_ARGS_PASS_FIRST_CALL_SIMPLE); });

    Target::init();

    compiler = activateCompiler<PascalCompiler>(*this, "org.pascal.fpc"s, { ".pas", ".pp" });
    if (!compiler)
        throw SW_RUNTIME_ERROR("No Pascal compiler found");

    compiler->Extension = getBuildSettings().TargetOS.getExecutableExtension();
    compiler->setOutputFile(getBaseOutputFileName(*this, {}, "bin"));

    SW_RETURN_MULTIPASS_END(init_pass);
}

Commands PascalTarget::getCommands1() const
{
    // fpc hello.adb

    // how to change output file?
    // works:
    // gnatmake -o ... input.adb

    for (auto f : gatherSourceFiles<SourceFile>(*this, { ".pas", ".pp" }))
        compiler->addSourceFile(f->file);
    Commands cmds;
    auto c = compiler->getCommand(*this);
    c->working_directory = getObjectDir();
    cmds.insert(c);
    return cmds;
}

PythonLibrary::PythonLibrary(TargetBase &parent, const PackageId &id)
    : Target(parent, id), SourceFileTargetOptions(*this)
{
}

bool PythonLibrary::init()
{
    auto r = Target::init();
    return r;
}

Files PythonLibrary::gatherAllFiles() const
{
    Files files;
    for (auto &f : *this)
        files.insert(f.first);
    return files;
}

}
