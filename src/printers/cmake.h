#pragma once

#include "../config.h"
#include "printer.h"

struct CMakePrinter : Printer
{
    void prepare_build(path fn, const String &cppan) override;
    int generate() const override;
    int build() const override;

    void print() override;

    void clear_cache(path p) const override;
    void clear_exports(path p) const override;

private:
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
