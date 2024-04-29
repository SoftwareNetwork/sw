// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2020 Egor Pugin <egor.pugin@gmail.com>

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
    Strings args_expanded;
    String version;
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
