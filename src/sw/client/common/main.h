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

#include <primitives/string.h>

#include <functional>
#include <memory>
#include <optional>

struct Options;
struct ClOptions;

struct SW_CLIENT_COMMON_API StartupData
{
    using UserFunction = std::function<bool /* stop execution */(StartupData &)>;

    int argc;
    char **argv;

    String program_short_name; // used in updater
    String overview;
    Strings args;
    std::unique_ptr<ClOptions> cloptions;
    std::unique_ptr<Options> options;
    std::optional<int> exit_code;

    UserFunction after_create_options;
    UserFunction after_setup;

    StartupData(int argc, char **argv);
    ~StartupData();

    // meta call
    int run();

    // call in this order
    void prepareArgs();
    void parseArgs();
    void createOptions();
    void setup();
    void sw_main();

    Options &getOptions();
    ClOptions &getClOptions();

private:
    int exit(int);
    int builtinCall();
    void initLogger();
    void setWorkingDir();
    void setHttpSettings();
};
