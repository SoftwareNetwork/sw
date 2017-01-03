/*
 * Copyright (C) 2016-2017, Egor Pugin
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "context.h"
#include "checks.h"
#include "project.h"

#define CPP_HEADER_FILENAME "cppan.h"

#define CPPAN_EXPORT "CPPAN_EXPORT"
#define CPPAN_EXPORT_PREFIX "CPPAN_API_"
#define CPPAN_PROLOG "CPPAN_PROLOG"
#define CPPAN_EPILOG "CPPAN_EPILOG"

#define CPPAN_LOCAL_BUILD_PREFIX "cppan-build-"
#define CPPAN_CONFIG_FILENAME "config.cmake"

extern const Strings configuration_types;
extern const Strings configuration_types_normal;
extern const Strings configuration_types_no_rel;

enum class PrinterType
{
    CMake,
    //Ninja,
    // add more here
};

struct BuildSettings;
struct Config;
struct Directories;
struct Settings;

struct Printer
{
    Package d;
    class AccessTable *access_table = nullptr;
    path cwd;
    Settings &settings;

    Printer();

    virtual void prepare_build(const BuildSettings &bs) const = 0;
    virtual void prepare_rebuild() const = 0;
    virtual int generate(const BuildSettings &bs) const = 0;
    virtual int build(const BuildSettings &bs) const = 0;

    virtual void print() const = 0;
    virtual void print_meta() const = 0;

    virtual void clear_cache() const = 0;
    virtual void clear_exports() const = 0;
    virtual void clear_export(const path &p) const = 0;

    virtual void parallel_vars_check(const ParallelCheckOptions &options) const = 0;

    static std::unique_ptr<Printer> create(PrinterType type);
};
