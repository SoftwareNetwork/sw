// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2016-2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include <sw/support/filesystem.h>

#include <primitives/yaml.h>

namespace sw::cppan
{

yaml load_yaml_config(const path &p);
yaml load_yaml_config(const String &s);
yaml load_yaml_config(yaml &root);

void dump_yaml_config(const path &p, const yaml &root);
String dump_yaml_config(const yaml &root);

}
