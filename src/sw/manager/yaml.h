// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2016-2018 Egor Pugin

#pragma once

#include <sw/support/filesystem.h>

#include <primitives/yaml.h>

yaml load_yaml_config(const path &p);
yaml load_yaml_config(const String &s);

void dump_yaml_config(const path &p, const yaml &root);
String dump_yaml_config(const yaml &root);
