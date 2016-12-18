/*
 * Copyright (c) 2016, Egor Pugin
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     1. Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *     2. Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the distribution.
 *     3. Neither the name of the copyright holder nor the names of
 *        its contributors may be used to endorse or promote products
 *        derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "config.h"
#include "printer.h"

void file_header(Context &ctx, const Package &d, bool root = false);
void file_footer(Context &ctx, const Package &d);

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

    void parallel_vars_check(const path &dir, const path &vars_file, const path &checks_file, const String &generator, const String &toolchain = String()) const override;

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
    void print_bs_insertion(Context &ctx, const Project &p, const String &name, const String BuildSystemConfigInsertions::*i) const;
    void print_source_groups(Context &ctx, const path &dir) const;

    bool must_update_contents(const path &fn) const;
    void write_if_older(const path &fn, const String &s) const;
};
