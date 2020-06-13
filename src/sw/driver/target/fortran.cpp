// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#include "fortran.h"

#include "common.h"

#include "sw/driver/build.h"
#include "sw/driver/compiler/detect.h"

#include <sw/core/sw_context.h>
#include <sw/manager/storage.h>

namespace sw
{

void detectFortranCompilers(DETECT_ARGS)
{
    // TODO: gfortran, flang, ifort, pgfortran, f90 (Oracle Sun), xlf, bgxlf, ...
    // aocc, armflang

    // TODO: add each program separately

    auto p = std::make_shared<SimpleProgram>();
    auto f = resolveExecutable("gfortran");
    if (!fs::exists(f))
    {
        auto f = resolveExecutable("f95");
        if (!fs::exists(f))
        {
            auto f = resolveExecutable("g95");
            if (!fs::exists(f))
                return;
        }
    }
    p->file = f;

    auto v = getVersion(s, p->file);
    addProgram(DETECT_ARGS_PASS, PackageId("org.gnu.gcc.fortran", v), {}, p);
}

FortranTarget::FortranTarget(TargetBase &parent, const PackageId &id)
    : Target(parent, id), NativeTargetOptionsGroup((Target &)*this)
{
}

bool FortranTarget::init()
{
    static std::once_flag f;
    std::call_once(f, [this] {detectFortranCompilers(DETECT_ARGS_PASS_FIRST_CALL_SIMPLE); });

    Target::init();

    /*C->input_extensions = {
    ".f",
    ".FOR",
    ".for",
    ".f77",
    ".f90",
    ".f95",

    // support Preprocessing
    ".F",
    ".fpp",
    ".FPP",
    };*/
    compiler = activateCompiler<decltype(compiler)::element_type>(*this, "org.gnu.gcc.fortran"s, { ".f" });
    if (!compiler)
        throw SW_RUNTIME_ERROR("No Fortran compiler found");

    compiler->Extension = getBuildSettings().TargetOS.getExecutableExtension();
    compiler->setOutputFile(getBaseOutputFileName(*this, {}, "bin"));

    SW_RETURN_MULTIPASS_END(init_pass);
}

Commands FortranTarget::getCommands1() const
{
    auto get_output_file = [this](const path &in)
    {
        return BinaryDir.parent_path() / "obj" / SourceFile::getObjectFilename(*this, in);
    };

    Commands cmds;
    for (auto f : gatherSourceFiles<SourceFile>(*this, { ".f" }))
    {
        auto c = std::dynamic_pointer_cast<decltype(compiler)::element_type>(compiler->clone());
        c->setSourceFile(f->file);
        c->Extension = getBuildSettings().TargetOS.getObjectFileExtension();
        c->setOutputFile(get_output_file(f->file));

        auto cmd = c->getCommand(*this);
        cmd->push_back("-c"); // for gfortran
        cmds.insert(cmd);
    }

    // separate loop
    for (auto f : gatherSourceFiles<SourceFile>(*this, { ".f" }))
        compiler->setSourceFile(get_output_file(f->file) += getBuildSettings().TargetOS.getObjectFileExtension());
    cmds.insert(compiler->getCommand(*this));
    return cmds;
}

Commands FortranStaticLibrary::getCommands1() const
{
    // create ar here
    SW_UNIMPLEMENTED;

    // it's better to use common facilities from native targets

    Commands cmds;
    return cmds;
}

Commands FortranSharedLibrary::getCommands1() const
{
    compiler->Extension = getBuildSettings().TargetOS.getSharedLibraryExtension();
    compiler->setOutputFile(getBaseOutputFileName(*this, {}, "bin"));

    auto cmds = FortranTarget::getCommands1();
    compiler->getCommand(*this)->push_back("-shared");
    return cmds;
}

}
