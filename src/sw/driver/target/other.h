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

#pragma once

#include "native1.h"

namespace sw
{

// C#

struct SW_DRIVER_CPP_API CSharpTarget : Target
    , NativeTargetOptionsGroup
{
    CSharpTarget(TargetBase &parent, const PackageId &);

    SW_TARGET_USING_ASSIGN_OPS(NativeTargetOptionsGroup);

    std::shared_ptr<CSharpCompiler> compiler;

    TargetType getType() const override { return TargetType::CSharpLibrary; }

    bool init() override;
    DependenciesType gatherDependencies() const override { return NativeTargetOptionsGroup::gatherDependencies(); }
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
    RustTarget(TargetBase &parent, const PackageId &);

    SW_TARGET_USING_ASSIGN_OPS(NativeTargetOptionsGroup);

    std::shared_ptr<RustCompiler> compiler;

    TargetType getType() const override { return TargetType::RustLibrary; }

    bool init() override;
    DependenciesType gatherDependencies() const override { return NativeTargetOptionsGroup::gatherDependencies(); }
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
    GoTarget(TargetBase &parent, const PackageId &);

    SW_TARGET_USING_ASSIGN_OPS(NativeTargetOptionsGroup);

    std::shared_ptr<GoCompiler> compiler;

    TargetType getType() const override { return TargetType::GoLibrary; }

    bool init() override;
    DependenciesType gatherDependencies() const override { return NativeTargetOptionsGroup::gatherDependencies(); }
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

// Fortran

struct SW_DRIVER_CPP_API FortranTarget : Target
    , NativeTargetOptionsGroup
{
    FortranTarget(TargetBase &parent, const PackageId &);

    SW_TARGET_USING_ASSIGN_OPS(NativeTargetOptionsGroup);

    std::shared_ptr<FortranCompiler> compiler;

    TargetType getType() const override { return TargetType::FortranLibrary; }

    bool init() override;
    DependenciesType gatherDependencies() const override { return NativeTargetOptionsGroup::gatherDependencies(); }
    Files gatherAllFiles() const override { return NativeTargetOptionsGroup::gatherAllFiles(); }

private:
    Commands getCommands1() const override;
};

struct SW_DRIVER_CPP_API FortranExecutable : FortranTarget
{
    using Base = FortranTarget;
    using Base::Base;
    TargetType getType() const override { return TargetType::FortranExecutable; }
};

// Java

struct SW_DRIVER_CPP_API JavaTarget : Target
    , NativeTargetOptionsGroup
{
    JavaTarget(TargetBase &parent, const PackageId &);

    SW_TARGET_USING_ASSIGN_OPS(NativeTargetOptionsGroup);

    std::shared_ptr<JavaCompiler> compiler;

    TargetType getType() const override { return TargetType::JavaLibrary; }

    bool init() override;
    DependenciesType gatherDependencies() const override { return NativeTargetOptionsGroup::gatherDependencies(); }
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
    KotlinTarget(TargetBase &parent, const PackageId &);

    SW_TARGET_USING_ASSIGN_OPS(NativeTargetOptionsGroup);

    std::shared_ptr<KotlinCompiler> compiler;

    TargetType getType() const override { return TargetType::KotlinLibrary; }

    bool init() override;
    DependenciesType gatherDependencies() const override { return NativeTargetOptionsGroup::gatherDependencies(); }
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
    DTarget(TargetBase &parent, const PackageId &);

    SW_TARGET_USING_ASSIGN_OPS(NativeTargetOptionsGroup);

    std::shared_ptr<DCompiler> compiler;

    TargetType getType() const override { return TargetType::DLibrary; }

    bool init() override;
    DependenciesType gatherDependencies() const override { return NativeTargetOptionsGroup::gatherDependencies(); }
    Files gatherAllFiles() const override { return NativeTargetOptionsGroup::gatherAllFiles(); }

private:
    Commands getCommands1() const override;

    //
    bool isStaticLibrary() const override { return false; }
    NativeLinker *getSelectedTool() const override;
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
    bool init() override;
    TargetType getType() const override { return TargetType::DStaticLibrary; }

    bool isStaticLibrary() const override { return true; }
};

struct SW_DRIVER_CPP_API DSharedLibrary : DLibrary
{
    using Base = DLibrary;
    using Base::Base;
    bool init() override;
    TargetType getType() const override { return TargetType::DSharedLibrary; }
};

struct SW_DRIVER_CPP_API DExecutable : DTarget
{
    using Base = DTarget;
    using Base::Base;
    bool init() override;
    TargetType getType() const override { return TargetType::DExecutable; }
};

// Python

struct SW_DRIVER_CPP_API PythonLibrary : Target
    , SourceFileTargetOptions
{
    PythonLibrary(TargetBase &parent, const PackageId &);

    bool init() override;
    Files gatherAllFiles() const override;
};

}
