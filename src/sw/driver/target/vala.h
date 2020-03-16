/*
 * SW - Build System and Package Manager
 * Copyright (C) 2020 Egor Pugin
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

#include "native.h"

namespace sw
{

namespace detail
{

struct SW_DRIVER_CPP_API ValaBase
{
    virtual ~ValaBase();

    void init();
    void prepare();

protected:
    DependencyPtr d;
    std::shared_ptr<ValaCompiler> compiler;
    void getCommands(Commands &) const;
};

}

#define VALA_TYPE(t)                                               \
    struct SW_DRIVER_CPP_API Vala##t : t##Target, detail::ValaBase \
    {                                                              \
        using Base = t##Target;                                    \
        bool init() override;                                      \
        bool prepare() override;                                   \
                                                                   \
    private:                                                       \
        Commands getCommands1() const override;                    \
    }

VALA_TYPE(Library);
VALA_TYPE(StaticLibrary);
VALA_TYPE(SharedLibrary);
VALA_TYPE(Executable);

#undef VALA_TYPE

}
