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
    TargetType getType() const override { return TargetType::CSharpExecutable; }
};

// Rust

struct SW_DRIVER_CPP_API RustTarget : Target
    , NativeTargetOptionsGroup
{
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
    TargetType getType() const override { return TargetType::RustExecutable; }
};

// Go

struct SW_DRIVER_CPP_API GoTarget : Target
    , NativeTargetOptionsGroup
{
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
    TargetType getType() const override { return TargetType::GoExecutable; }
};

// Fortran

struct SW_DRIVER_CPP_API FortranTarget : Target
    , NativeTargetOptionsGroup
{
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
    TargetType getType() const override { return TargetType::FortranExecutable; }
};

// Java

struct SW_DRIVER_CPP_API JavaTarget : Target
    , NativeTargetOptionsGroup
{
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
    TargetType getType() const override { return TargetType::JavaExecutable; }
};

// Kotlin

struct SW_DRIVER_CPP_API KotlinTarget : Target
    , NativeTargetOptionsGroup
{
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
    TargetType getType() const override { return TargetType::KotlinExecutable; }
};

// D

struct SW_DRIVER_CPP_API DTarget : NativeTarget
    , NativeTargetOptionsGroup
{
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
};

struct SW_DRIVER_CPP_API DStaticLibrary : DLibrary
{
    bool init() override;
    TargetType getType() const override { return TargetType::DStaticLibrary; }

    bool isStaticLibrary() const override { return true; }
};

struct SW_DRIVER_CPP_API DSharedLibrary : DLibrary
{
    bool init() override;
    TargetType getType() const override { return TargetType::DSharedLibrary; }
};

struct SW_DRIVER_CPP_API DExecutable : DTarget
{
    bool init() override;
    TargetType getType() const override { return TargetType::DExecutable; }
};

}
