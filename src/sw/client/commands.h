#pragma once

#include <primitives/sw/cl.h>
#include <sw/builder/sw_context.h>

#define SUBCOMMAND_DECL(n) void cli_##n()
#define SUBCOMMAND_DECL2(n) void cli_##n(sw::SwContext &swctx)
#define SUBCOMMAND(n, d) SUBCOMMAND_DECL(n); SUBCOMMAND_DECL2(n);
#include "commands.inl"
#undef SUBCOMMAND

#define SUBCOMMAND(n, d) extern ::cl::SubCommand subcommand_##n;
#include "commands.inl"
#undef SUBCOMMAND

sw::SwContext createSwContext();
