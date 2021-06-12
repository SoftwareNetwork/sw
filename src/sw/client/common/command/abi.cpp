// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2020 Egor Pugin <egor.pugin@gmail.com>

#include "../commands.h"

//#include <sw/builder/program.h>
//#include <sw/core/input.h>
#include <sw/manager/storage.h>
#include <sw/support/filesystem.h>

#include <regex>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "abi");

// libabigail for ELF

SUBCOMMAND_DECL(abi)
{
#ifndef _WIN32
    SW_UNIMPLEMENTED;
#endif

    auto b = createBuildAndPrepare(getInputs());
    SW_UNIMPLEMENTED;
    //auto tgts1 = b->getTargetsToBuild();
    //b->build();

    /*auto i = getContext().getPredefinedTargets().find(sw::UnresolvedPackage("com.Microsoft.VisualStudio.VC.dumpbin-*"));
    if (i == getContext().getPredefinedTargets().end() || i->second.empty())
        throw SW_RUNTIME_ERROR("No dumpbin program");
    auto j = i->second.end() - 1;*/
    SW_UNIMPLEMENTED;
    /*auto p = (*j)->as<const sw::PredefinedProgram *>();
    if (!p)
        throw SW_RUNTIME_ERROR("No dumpbin program set");
    //auto &ds = (*j)->getInterfaceSettings();
    //if (!ds["output_file"])
        //throw SW_RUNTIME_ERROR("No dumpbin 'output_file' set");

    for (auto &[pkg, tgts] : tgts1)
    {
        for (auto &tgt : tgts)
        {
            auto &s = tgt->getInterfaceSettings();
            if (!s["output_file"] || !fs::exists(s["output_file"].getValue()))
                continue;
            primitives::Command c = *p->getProgram().clone()->getCommand();
            c.push_back("/EXPORTS");
            c.push_back(s["output_file"].getValue());
            c.execute();

            // ordinal hint RVA name
            static std::regex r_dumpbin(R"((\d+)\s+([0-9A-Fa-f]+)\s+([0-9A-Fa-f]+)\s+(\S+)\s+)");

            Strings symbols;
            std::smatch m;
            auto str = c.out.text;
            while (std::regex_search(str, m, r_dumpbin))
            {
                symbols.push_back(m[4].str());
                str = m.suffix().str();
            }

            if (!symbols.empty())
            {
                LOG_INFO(logger, pkg.toString() + " symbol list:");
                for (auto &s : symbols)
                    LOG_INFO(logger, "    - " + s);
            }
        }
    }*/
}
