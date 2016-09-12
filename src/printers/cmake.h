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

#include "../config.h"
#include "printer.h"

struct CMakePrinter : Printer
{
    void prepare_rebuild() override;
    void prepare_build(const path &fn, const String &cppan) override;
    int generate() const override;
    int build() const override;

    void print() override;
    void print_meta() override;

    void clear_cache(path p) const override;
    void clear_exports(path p) const override;

private:
    int _generate(bool force = false) const;
    void print_configs();
    void print_helper_file(const path &fn) const;
    void print_include_guards_file(const path &fn) const;
    void print_meta_config_file(const path &fn) const;
    void print_package_config_file(const path &fn) const;
    void print_package_actions_file(const path &fn) const;
    void print_package_include_file(const path &fn) const;
    void print_object_config_file(const path &fn) const;
    void print_object_include_config_file(const path &fn) const;
    void print_object_export_file(const path &fn) const;
    void print_object_build_file(const path &fn) const;
    void print_bs_insertion(Context &ctx, const Project &p, const String &name, const String BuildSystemConfigInsertions::*i) const;
};
