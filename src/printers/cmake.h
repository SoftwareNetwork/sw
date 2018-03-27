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

#include "config.h"
#include "printer.h"

#include <package_store.h>

class CMakeContext;

void file_header(CMakeContext &ctx, const Package &d, bool root = false);
void file_footer(CMakeContext &ctx, const Package &d);

struct CMakePrinter : Printer
{
    void prepare_build(const BuildSettings &bs) const override;
    void prepare_rebuild() const override;
    int generate(const BuildSettings &bs) const override;
    int build(const BuildSettings &bs) const override;

    void print() const override;
    void print_meta() const override;

    void clear_cache() const override;
    void clear_exports() const override;
    void clear_export(const path &p) const override;

    void parallel_vars_check(const ParallelCheckOptions &options) const override;

private:
    mutable SourceGroups sgs;

    void print_configs() const;
    void print_helper_file(const path &fn) const;
    void print_meta_config_file(const path &fn) const;
    void print_src_config_file(const path &fn) const;
    void print_src_actions_file(const path &fn) const;
    void print_src_include_file(const path &fn) const;
    void print_obj_config_file(const path &fn) const;
    void print_obj_generate_file(const path &fn) const;
    void print_obj_export_file(const path &fn) const;
    void print_obj_build_file(const path &fn) const;
    void print_bs_insertion(CMakeContext &ctx, const Project &p, const String &name, const String BuildSystemConfigInsertions::*i) const;
    void print_source_groups(CMakeContext &ctx) const;

    void print_build_dependencies(CMakeContext &ctx, const String &target) const;
    void print_copy_dependencies(CMakeContext &ctx, const String &target) const;

    void print_references(CMakeContext &ctx) const;
    void print_settings(CMakeContext &ctx) const;

    bool must_update_contents(const path &fn) const;
    void write_if_older(const path &fn, const String &s) const;
};

template <class F>
void add_aliases(Context &ctx, const Package &d, bool all, const StringSet &aliases, F &&f)
{
    auto add_line = [&ctx](const auto &s)
    {
        if (!s.empty())
            ctx.addLine(s);
    };

    auto add_aliases = [&](const auto &delim)
    {
        Version ver = d.version;
        if (!ver.isBranch())
        {
            add_line(std::forward<F>(f)(d.ppath.toString(delim) + "-" + ver.toAnyVersion(), ver));
            ver.patch = -1;
            add_line(std::forward<F>(f)(d.ppath.toString(delim) + "-" + ver.toAnyVersion(), ver));
            ver.minor = -1;
            add_line(std::forward<F>(f)(d.ppath.toString(delim) + "-" + ver.toAnyVersion(), ver));
        }
        else if (all)
            add_line(std::forward<F>(f)(d.ppath.toString(delim) + "-" + ver.toAnyVersion(), ver));
        add_line(std::forward<F>(f)(d.ppath.toString(delim), ver));
        ctx.addLine();
    };
    add_aliases(".");
    add_aliases("::");

    if (!aliases.empty())
    {
        ctx.addLine("# user-defined");
        for (auto &a : aliases)
        {
            if (!a.empty())
                add_line(std::forward<F>(f)(a, Version()));
        }
        ctx.addLine();
    }
}

template <class F>
void add_aliases(Context &ctx, const Package &d, bool all, F &&f)
{
    const auto &aliases = rd[d].config->getDefaultProject().aliases;
    add_aliases(ctx, d, all, aliases, std::forward<F>(f));
}

template <class F>
void add_aliases(Context &ctx, const Package &d, F &&f)
{
    add_aliases(ctx, d, true, std::forward<F>(f));
}

void registerCmakePackage();
