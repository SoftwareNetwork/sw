// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "native1.h"

namespace sw
{

// Ada

struct SW_DRIVER_CPP_API AdaTarget : Target
    , NativeTargetOptionsGroup
{
    //std::shared_ptr<AdaCompiler> compiler;

    AdaTarget(TargetBase &parent, const PackageName &);

    SW_TARGET_USING_ASSIGN_OPS(NativeTargetOptionsGroup);

    void init() override;
    std::set<Dependency*> gatherDependencies() const override { return NativeTargetOptionsGroup::gatherDependencies(); }
    Files gatherAllFiles() const override { return NativeTargetOptionsGroup::gatherAllFiles(); }

private:
    Commands getCommands1() const override;
};

struct SW_DRIVER_CPP_API AdaExecutable : AdaTarget
{
    using Base = AdaTarget;
    using Base::Base;
};

// C#

struct SW_DRIVER_CPP_API CSharpTarget : Target
    , NativeTargetOptionsGroup
{
    //std::shared_ptr<CSharpCompiler> compiler;

    CSharpTarget(TargetBase &parent, const PackageName &);

    SW_TARGET_USING_ASSIGN_OPS(NativeTargetOptionsGroup);

    TargetType getType() const override { return TargetType::CSharpLibrary; }

    void init() override;
    std::set<Dependency*> gatherDependencies() const override { return NativeTargetOptionsGroup::gatherDependencies(); }
    Files gatherAllFiles() const override { return NativeTargetOptionsGroup::gatherAllFiles(); }

private:
    Commands getCommands1() const override;
};

struct SW_DRIVER_CPP_API CSharpExecutable : CSharpTarget
{
    using Base = CSharpTarget;
    using Base::Base;

    TargetType getType() const override { return TargetType::CSharpExecutable; }
};

// Rust

struct SW_DRIVER_CPP_API RustTarget : Target
    , NativeTargetOptionsGroup
{
    //std::shared_ptr<RustCompiler> compiler;

    RustTarget(TargetBase &parent, const PackageName &);

    SW_TARGET_USING_ASSIGN_OPS(NativeTargetOptionsGroup);

    TargetType getType() const override { return TargetType::RustLibrary; }

    void init() override;
    std::set<Dependency*> gatherDependencies() const override { return NativeTargetOptionsGroup::gatherDependencies(); }
    Files gatherAllFiles() const override { return NativeTargetOptionsGroup::gatherAllFiles(); }

private:
    Commands getCommands1() const override;
};

struct SW_DRIVER_CPP_API RustExecutable : RustTarget
{
    using Base = RustTarget;
    using Base::Base;
    TargetType getType() const override { return TargetType::RustExecutable; }
};

// Go

struct SW_DRIVER_CPP_API GoTarget : Target
    , NativeTargetOptionsGroup
{
    //std::shared_ptr<GoCompiler> compiler;

    GoTarget(TargetBase &parent, const PackageName &);

    SW_TARGET_USING_ASSIGN_OPS(NativeTargetOptionsGroup);

    TargetType getType() const override { return TargetType::GoLibrary; }

    void init() override;
    std::set<Dependency*> gatherDependencies() const override { return NativeTargetOptionsGroup::gatherDependencies(); }
    Files gatherAllFiles() const override { return NativeTargetOptionsGroup::gatherAllFiles(); }

private:
    Commands getCommands1() const override;
};

struct SW_DRIVER_CPP_API GoExecutable : GoTarget
{
    using Base = GoTarget;
    using Base::Base;
    TargetType getType() const override { return TargetType::GoExecutable; }
};

// Java

struct SW_DRIVER_CPP_API JavaTarget : Target
    , NativeTargetOptionsGroup
{
    //std::shared_ptr<JavaCompiler> compiler;

    JavaTarget(TargetBase &parent, const PackageName &);

    SW_TARGET_USING_ASSIGN_OPS(NativeTargetOptionsGroup);

    TargetType getType() const override { return TargetType::JavaLibrary; }

    void init() override;
    std::set<Dependency*> gatherDependencies() const override { return NativeTargetOptionsGroup::gatherDependencies(); }
    Files gatherAllFiles() const override { return NativeTargetOptionsGroup::gatherAllFiles(); }

private:
    Commands getCommands1() const override;
};

struct SW_DRIVER_CPP_API JavaExecutable : JavaTarget
{
    using Base = JavaTarget;
    using Base::Base;
    TargetType getType() const override { return TargetType::JavaExecutable; }
};

// Kotlin

struct SW_DRIVER_CPP_API KotlinTarget : Target
    , NativeTargetOptionsGroup
{
    //std::shared_ptr<KotlinCompiler> compiler;

    KotlinTarget(TargetBase &parent, const PackageName &);

    SW_TARGET_USING_ASSIGN_OPS(NativeTargetOptionsGroup);

    TargetType getType() const override { return TargetType::KotlinLibrary; }

    void init() override;
    std::set<Dependency*> gatherDependencies() const override { return NativeTargetOptionsGroup::gatherDependencies(); }
    Files gatherAllFiles() const override { return NativeTargetOptionsGroup::gatherAllFiles(); }

private:
    Commands getCommands1() const override;
};

struct SW_DRIVER_CPP_API KotlinExecutable : KotlinTarget
{
    using Base = KotlinTarget;
    using Base::Base;
    TargetType getType() const override { return TargetType::KotlinExecutable; }
};

// D

struct SW_DRIVER_CPP_API DTarget : NativeTarget
    , NativeTargetOptionsGroup
{
    //std::shared_ptr<DCompiler> compiler;

    DTarget(TargetBase &parent, const PackageName &);

    SW_TARGET_USING_ASSIGN_OPS(NativeTargetOptionsGroup);

    TargetType getType() const override { return TargetType::DLibrary; }

    void init() override;
    std::set<Dependency*> gatherDependencies() const override { return NativeTargetOptionsGroup::gatherDependencies(); }
    Files gatherAllFiles() const override { return NativeTargetOptionsGroup::gatherAllFiles(); }

private:
    Commands getCommands1() const override;

    //
    bool isStaticLibrary() const override { return false; }
    //NativeLinker *getSelectedTool() const override;
};

struct SW_DRIVER_CPP_API DLibrary : DTarget
{
    using Base = DTarget;
    using Base::Base;
};

struct SW_DRIVER_CPP_API DStaticLibrary : DLibrary
{
    using Base = DLibrary;
    using Base::Base;
    void init() override;
    TargetType getType() const override { return TargetType::DStaticLibrary; }

    bool isStaticLibrary() const override { return true; }
};

struct SW_DRIVER_CPP_API DSharedLibrary : DLibrary
{
    using Base = DLibrary;
    using Base::Base;
    void init() override;
    TargetType getType() const override { return TargetType::DSharedLibrary; }
};

struct SW_DRIVER_CPP_API DExecutable : DTarget
{
    using Base = DTarget;
    using Base::Base;
    void init() override;
    TargetType getType() const override { return TargetType::DExecutable; }
};

// Pascal

struct SW_DRIVER_CPP_API PascalTarget : Target
    , NativeTargetOptionsGroup
{
    //std::shared_ptr<PascalCompiler> compiler;

    PascalTarget(TargetBase &parent, const PackageName &);

    SW_TARGET_USING_ASSIGN_OPS(NativeTargetOptionsGroup);

    void init() override;
    std::set<Dependency*> gatherDependencies() const override { return NativeTargetOptionsGroup::gatherDependencies(); }
    Files gatherAllFiles() const override { return NativeTargetOptionsGroup::gatherAllFiles(); }

private:
    Commands getCommands1() const override;
};

struct SW_DRIVER_CPP_API PascalExecutable : PascalTarget
{
    using Base = PascalTarget;
    using Base::Base;
};

// Python

struct SW_DRIVER_CPP_API PythonLibrary : Target
    , SourceFileTargetOptions
{
    PythonLibrary(TargetBase &parent, const PackageName &);

    void init() override;
    Files gatherAllFiles() const override;
};

}
