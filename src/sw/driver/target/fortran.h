// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "base.h"

namespace sw
{

// Fortran

struct SW_DRIVER_CPP_API FortranTarget : Target
    , NativeTargetOptionsGroup
{
    //std::shared_ptr<FortranCompiler> compiler;

    FortranTarget(TargetBase &parent, const PackageId &);

    SW_TARGET_USING_ASSIGN_OPS(NativeTargetOptionsGroup);

    TargetType getType() const override { return TargetType::FortranLibrary; }

    void init() override;
    DependenciesType gatherDependencies() const override { return NativeTargetOptionsGroup::gatherDependencies(); }
    Files gatherAllFiles() const override { return NativeTargetOptionsGroup::gatherAllFiles(); }

protected:
    Commands getCommands1() const override;
};

struct SW_DRIVER_CPP_API FortranLibrary : FortranTarget
{
    using Base = FortranTarget;
    using Base::Base;
};

struct SW_DRIVER_CPP_API FortranStaticLibrary : FortranLibrary
{
    using Base = FortranLibrary;
    using Base::Base;

private:
    Commands getCommands1() const override;
};

struct SW_DRIVER_CPP_API FortranSharedLibrary : FortranLibrary
{
    using Base = FortranLibrary;
    using Base::Base;

private:
    Commands getCommands1() const override;
};

struct SW_DRIVER_CPP_API FortranExecutable : FortranTarget
{
    using Base = FortranTarget;
    using Base::Base;
    TargetType getType() const override { return TargetType::FortranExecutable; }
};

}
