// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2017-2019 Egor Pugin <egor.pugin@gmail.com>

#include "../commands.h"
#include "../generator/generator.h"

#include <sw/builder/execution_plan.h>
#include <sw/core/build.h>
#include <sw/core/input.h>

String vs_zero_check_stamp_ext = ".stamp";

SUBCOMMAND_DECL(generate)
{
    if (!getOptions().options_generate.check_stamp_list.empty())
    {
        auto stampfn = path(getOptions().options_generate.check_stamp_list) += vs_zero_check_stamp_ext;
        auto files = read_lines(getOptions().options_generate.check_stamp_list);
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

    // actual generate
    if (getOptions().options_generate.generator.empty())
    {
#ifdef _WIN32
        getOptions().options_generate.generator = "vs";
#endif
    }

    auto generator = Generator::create(getOptions());
    if (generator->getType() == GeneratorType::VisualStudio)
    {
        auto &compilers = (Strings&)getOptions().compiler;
        if (compilers.empty())
        {
            getOptions().compiler.push_back("msvc");
        }
        if (getOptions().configuration.empty())
        {
            getOptions().configuration.push_back("d");
            getOptions().configuration.push_back("rwdi");
            getOptions().configuration.push_back("r");
        }
        // vs gen works only with this atm
        auto has_msvc = std::ranges::any_of(getOptions().compiler, [](auto &v){return v.contains("msvc");});
        auto has_clang_or_clang_cl = std::ranges::any_of(getOptions().compiler, [](auto &v){return v.contains("clang");});
        if (false
            || has_msvc
            || !has_clang_or_clang_cl
            // what about gnu?
            ) {
            // not for clang, currently some packages can't be built with it (python, bison)
            getOptions().use_same_config_for_host_dependencies = true;
        }

        auto g = (VSGenerator*)generator.get();
        if (getOptions().options_generate.print_overridden_dependencies)
            g->add_overridden_packages = true;
        if (getOptions().options_generate.print_dependencies)
            g->add_all_packages = true;
    }

    auto b = createBuildAndPrepare({getInputs(), getOptions().input_settings_pairs});
    b->getExecutionPlan(); // prepare commands
    generator->generate(*b);
}
