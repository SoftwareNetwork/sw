// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "native.h"

 // vala.CompilerOptions? vala.copts?
#define VALA_OPTIONS_NAME "vala"

namespace sw
{

/*namespace detail
{

struct SW_DRIVER_CPP_API ValaBase
{
    virtual ~ValaBase();

    void init();
    void prepare();

    path getOutputCCodeFileName(const path &valasrc) const;

protected:
    DependencyPtr d;
    //std::shared_ptr<ValaCompiler> compiler;
    path OutputDir;
    NativeCompiledTarget *t_ = nullptr;
    void getCommands1(Commands &) const;
};

}

#define VALA_TYPE(t)                                               \
    struct SW_DRIVER_CPP_API Vala##t : t##Target, detail::ValaBase \
    {                                                              \
        using Base = t##Target;                                    \
        using Base::Base;                                          \
        void init() override;                                      \
        void prepare() override;                                   \
                                                                   \
    private:                                                       \
        Commands getCommands1() const override;                    \
    }

VALA_TYPE(Library);
VALA_TYPE(StaticLibrary);
VALA_TYPE(SharedLibrary);
VALA_TYPE(Executable);

#undef VALA_TYPE*/

}
