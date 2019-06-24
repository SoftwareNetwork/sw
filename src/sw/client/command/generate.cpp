// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "commands.h"
#include "../generator/generator.h"

extern ::cl::list<String> build_arg;

static ::cl::opt<String> build_arg_generate(::cl::Positional, ::cl::desc("File or directory to use to generate projects"), ::cl::init("."), ::cl::sub(subcommand_generate));

extern String gGenerator;
::cl::opt<String, true> cl_generator("G", ::cl::desc("Generator"), ::cl::location(gGenerator), ::cl::sub(subcommand_generate));
::cl::alias generator2("g", ::cl::desc("Alias for -G"), ::cl::aliasopt(cl_generator));
extern bool gPrintDependencies;
static ::cl::opt<bool, true> print_dependencies("print-dependencies", ::cl::location(gPrintDependencies), ::cl::sub(subcommand_generate));
// ad = all deps?
::cl::alias print_dependencies4("ad", ::cl::desc("Alias for -print-dependencies"), ::cl::aliasopt(print_dependencies));
::cl::alias print_dependencies2("d", ::cl::desc("Alias for -print-dependencies"), ::cl::aliasopt(print_dependencies));
::cl::alias print_dependencies3("deps", ::cl::desc("Alias for -print-dependencies"), ::cl::aliasopt(print_dependencies));
extern bool gPrintOverriddenDependencies;
static ::cl::opt<bool, true> print_overridden_dependencies("print-overridden-dependencies", ::cl::location(gPrintOverriddenDependencies), ::cl::sub(subcommand_generate));
// o = od?
::cl::alias print_overridden_dependencies4("o", ::cl::desc("Alias for -print-overridden-dependencies"), ::cl::aliasopt(print_overridden_dependencies));
::cl::alias print_overridden_dependencies2("od", ::cl::desc("Alias for -print-overridden-dependencies"), ::cl::aliasopt(print_overridden_dependencies));
::cl::alias print_overridden_dependencies3("odeps", ::cl::desc("Alias for -print-overridden-dependencies"), ::cl::aliasopt(print_overridden_dependencies));
extern bool gOutputNoConfigSubdir;
static ::cl::opt<bool, true> output_no_config_subdir("output-no-config-subdir", ::cl::location(gOutputNoConfigSubdir), ::cl::sub(subcommand_generate));

// generated solution dir instead of .sw/...
//static ::cl::opt<String> generate_binary_dir("B", ::cl::desc("Explicitly specify a build directory."), ::cl::sub(subcommand_build), ::cl::init(SW_BINARY_DIR));

SUBCOMMAND_DECL(generate)
{
    auto swctx = createSwContext();
    cli_generate(*swctx);
}

SUBCOMMAND_DECL2(generate)
{
    if (gGenerator.empty())
    {
#ifdef _WIN32
        gGenerator = "vs";
#endif
    }
    ((Strings&)build_arg).clear();
    build_arg.push_back(build_arg_generate.getValue());

    swctx.load((Strings&)build_arg);
    swctx.configure();

    auto generator = sw::Generator::create(gGenerator);
    generator->generate(swctx);
}
