// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2017-2019 Egor Pugin <egor.pugin@gmail.com>

#ifndef COMMA
#define COMMA
#define SW_COMMA_SELF
#endif

/*
commands are:

test file/dir profile - test something. profile - test actions, default - test
profiles: all, bench, test, ..., doc? ...

run file/dir/pkg - run the selected program or an executable package directly,
run knows how to run pkg (e.g. if we run python package it will select py interpreter with correct version)

update - update packages db
upgrade - upgrade installed pkgs
self-upgrade - upgrade the client. implement via upgrade?
*/

SUBCOMMAND(abi) COMMA // rename? move to --option?
SUBCOMMAND(alias) COMMA
SUBCOMMAND(build) COMMA
//SUBCOMMAND(b) COMMA // alias for build
SUBCOMMAND(configure) COMMA
SUBCOMMAND(create) COMMA
SUBCOMMAND(doc) COMMA // invokes documentation (hopefully)
SUBCOMMAND(generate) COMMA
// rename to query?
SUBCOMMAND(get) COMMA // returns different information
SUBCOMMAND(fetch) COMMA
SUBCOMMAND(install) COMMA
//SUBCOMMAND(i) COMMA // alias for install
SUBCOMMAND(integrate) COMMA
SUBCOMMAND(list) COMMA
SUBCOMMAND(open) COMMA
SUBCOMMAND(override) COMMA
SUBCOMMAND(mirror) COMMA
SUBCOMMAND(pack) COMMA
SUBCOMMAND(path) COMMA
SUBCOMMAND(remote) COMMA
SUBCOMMAND(remove) COMMA
SUBCOMMAND(run) COMMA
SUBCOMMAND(server) COMMA
SUBCOMMAND(setup) COMMA
SUBCOMMAND(test) COMMA
SUBCOMMAND(update) COMMA // update lock file?
SUBCOMMAND(upload) COMMA
SUBCOMMAND(verify) COMMA
SUBCOMMAND(uri) COMMA

#ifdef SW_COMMA_SELF
#undef COMMA
#undef SW_COMMA_SELF
#endif
