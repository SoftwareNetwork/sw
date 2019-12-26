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
#include "../generator/generator.h"

#include <sw/builder/execution_plan.h>
#include <sw/core/build.h>
#include <sw/core/input.h>

DEFINE_SUBCOMMAND(generate, "Generate IDE projects.");

static ::cl::list<String> build_arg_generate(::cl::Positional, ::cl::desc("File or directory to use to generate projects"), ::cl::sub(subcommand_generate));

String gGenerator;
static ::cl::opt<String, true> cl_generator("G", ::cl::desc("Generator"), ::cl::location(gGenerator), ::cl::sub(subcommand_generate));
static ::cl::alias generator2("g", ::cl::desc("Alias for -G"), ::cl::aliasopt(cl_generator));
extern bool gPrintDependencies;
static ::cl::opt<bool, true> print_dependencies("print-dependencies", ::cl::location(gPrintDependencies), ::cl::sub(subcommand_generate));
// ad = all deps?
static ::cl::alias print_dependencies4("ad", ::cl::desc("Alias for -print-dependencies"), ::cl::aliasopt(print_dependencies));
static ::cl::alias print_dependencies2("d", ::cl::desc("Alias for -print-dependencies"), ::cl::aliasopt(print_dependencies));
static ::cl::alias print_dependencies3("deps", ::cl::desc("Alias for -print-dependencies"), ::cl::aliasopt(print_dependencies));
extern bool gPrintOverriddenDependencies;
static ::cl::opt<bool, true> print_overridden_dependencies("print-overridden-dependencies", ::cl::location(gPrintOverriddenDependencies), ::cl::sub(subcommand_generate));
// o = od?
static ::cl::alias print_overridden_dependencies4("o", ::cl::desc("Alias for -print-overridden-dependencies"), ::cl::aliasopt(print_overridden_dependencies));
static ::cl::alias print_overridden_dependencies2("od", ::cl::desc("Alias for -print-overridden-dependencies"), ::cl::aliasopt(print_overridden_dependencies));
static ::cl::alias print_overridden_dependencies3("odeps", ::cl::desc("Alias for -print-overridden-dependencies"), ::cl::aliasopt(print_overridden_dependencies));
extern bool gOutputNoConfigSubdir;
static ::cl::opt<bool, true> output_no_config_subdir("output-no-config-subdir", ::cl::location(gOutputNoConfigSubdir), ::cl::sub(subcommand_generate));

::cl::opt<path> check_stamp_list("check-stamp-list", ::cl::sub(subcommand_generate), ::cl::Hidden);
String vs_zero_check_stamp_ext = ".stamp";

// generated solution dir instead of .sw/...
//static ::cl::opt<String> generate_binary_dir("B", ::cl::desc("Explicitly specify a build directory."), ::cl::sub(subcommand_build), ::cl::init(SW_BINARY_DIR));

extern ::cl::list<String> compiler;
extern ::cl::list<String> configuration;

SUBCOMMAND_DECL(generate)
{
    if (!check_stamp_list.empty())
    {
        auto stampfn = path(check_stamp_list) += vs_zero_check_stamp_ext;
        auto files = read_lines(check_stamp_list);
        uint64_t mtime = 0;
        bool missing = false;
        for (auto &f : files)
        {
            if (!fs::exists(f))
            {
                mtime ^= 0;
                continue;
            }
            auto lwt = fs::last_write_time(f);
            mtime ^= file_time_type2time_t(lwt);
        }
        if (fs::exists(stampfn))
        {
            auto t0 = std::stoull(read_file(stampfn));
            if (t0 == mtime)
            {
                // must write to file to make it updated!
                write_file(stampfn, std::to_string(mtime));
                return;
            }
        }
    }

    if (build_arg_generate.empty())
        build_arg_generate.push_back(".");

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

    auto generator = Generator::create(gGenerator);
    if (generator->getType() == GeneratorType::VisualStudio)
    {
        auto &compilers = (Strings&)compiler;
        if (!compilers.empty())
        {
            if (compilers.size() > 1)
                throw SW_RUNTIME_ERROR("Only one compiler may be specified");
        }
        else
            compiler.push_back("msvc");
        if (configuration.empty())
        {
            configuration.push_back("d");
            configuration.push_back("rwdi");
            configuration.push_back("r");
        }
        auto hs = swctx.getHostSettings();
        hs["use_same_config_for_host_dependencies"] = "true";
        hs["use_same_config_for_host_dependencies"].useInHash(false);
        swctx.setHostSettings(hs);
    }

    auto b = setBuildArgsAndCreateBuildAndPrepare(swctx, (Strings&)build_arg_generate);
    b->getExecutionPlan(); // prepare commands
    generator->generate(*b);
}
