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

String vs_zero_check_stamp_ext = ".stamp";

SUBCOMMAND_DECL(generate)
{
    if (!options.options_generate.check_stamp_list.empty())
    {
        auto stampfn = path(options.options_generate.check_stamp_list) += vs_zero_check_stamp_ext;
        auto files = read_lines(options.options_generate.check_stamp_list);
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

    if (options.options_generate.build_arg_generate.empty())
        options.options_generate.build_arg_generate.push_back(".");

    auto swctx = createSwContext(options);
    cli_generate(*swctx, options);
}

SUBCOMMAND_DECL2(generate)
{
    if (options.options_generate.generator.empty())
    {
#ifdef _WIN32
        options.options_generate.generator = "vs";
#endif
    }

    auto generator = Generator::create(options);
    if (generator->getType() == GeneratorType::VisualStudio)
    {
        auto &compilers = (Strings&)options.compiler;
        if (!compilers.empty())
        {
            if (compilers.size() > 1)
                throw SW_RUNTIME_ERROR("Only one compiler may be specified");
        }
        else
            options.compiler.push_back("msvc");
        if (options.configuration.empty())
        {
            options.configuration.push_back("d");
            options.configuration.push_back("rwdi");
            options.configuration.push_back("r");
        }
        auto hs = swctx.getHostSettings();
        hs["use_same_config_for_host_dependencies"] = "true";
        hs["use_same_config_for_host_dependencies"].useInHash(false);
        swctx.setHostSettings(hs);

        auto g = (VSGenerator*)generator.get();
        if (options.options_generate.print_overridden_dependencies)
            g->add_overridden_packages = true;
        if (options.options_generate.print_dependencies)
            g->add_all_packages = true;
    }

    auto b = createBuildAndPrepare(swctx, (Strings&)options.options_generate.build_arg_generate, options);
    b->getExecutionPlan(); // prepare commands
    generator->generate(*b);
}
