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

SUBCOMMAND(build) COMMA
SUBCOMMAND(b) COMMA // alias for build
SUBCOMMAND(configure) COMMA
SUBCOMMAND(create) COMMA
SUBCOMMAND(generate) COMMA
SUBCOMMAND(fetch) COMMA
SUBCOMMAND(install) COMMA
SUBCOMMAND(i) COMMA // alias for install
SUBCOMMAND(integrate) COMMA
SUBCOMMAND(list) COMMA
SUBCOMMAND(open) COMMA
SUBCOMMAND(override) COMMA
//SUBCOMMAND(mirror, "Manage software mirrors.") COMMA
//SUBCOMMAND(pack, "Used to prepare distribution packages.") COMMA
SUBCOMMAND(remote) COMMA
SUBCOMMAND(remove) COMMA
SUBCOMMAND(setup) COMMA
SUBCOMMAND(test) COMMA
SUBCOMMAND(update) COMMA
SUBCOMMAND(upload) COMMA
SUBCOMMAND(uri) COMMA

#ifdef SW_COMMA_SELF
#undef COMMA
#undef SW_COMMA_SELF
#endif
