/*
 * SW - Build System and Package Manager
 * Copyright (C) 2017-2019 Egor Pugin
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

#include "commands.h"

extern bool gWithTesting;
extern ::cl::list<String> build_arg;

::cl::list<String> build_arg_test(::cl::Positional, ::cl::desc("File or directory to use to generate projects"), ::cl::sub(subcommand_test));

SUBCOMMAND_DECL(test)
{
    auto swctx = createSwContext();
    gWithTesting = true;
    (Strings&)build_arg = (Strings&)build_arg_test;
    cli_build(*swctx);
}
