// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>

#include "yaml.h"

#include <boost/algorithm/string.hpp>

void prepare_config_for_reading(yaml &root)
{
    // can be all node checks from config, project, settings moved here?

    // no effect
    if (!root.IsMap())
        return;
}

yaml load_yaml_config(const path &p)
{
    auto s = read_file(p);
    return load_yaml_config(s);
}

yaml load_yaml_config(const String &s)
{
    auto root = YAML::Load(s);
    prepare_config_for_reading(root);
    return root;
}

void dump_yaml_config(const path &p, const yaml &root)
{
    write_file(p, dump_yaml_config(root));
}

String dump_yaml_config(const yaml &root)
{
    using namespace YAML;

    if (!root.IsMap())
        return Dump(root);

    Emitter e;
    e.SetIndent(4);
    e << BeginMap;

    auto emit = [&e](auto root, const String &k, bool literal = false)
    {
        e << Key << k;
        e << Value;
        if (literal)
        {
            e << Literal;
            e << boost::trim_copy(root[k].template as<String>());
        }
        else
            e << root[k];
        e << Newline << Newline;
    };

    auto print_rest = [&emit](auto root)
    {
        for (auto n : root)
        {
            auto k = n.first.template as<String>();
            emit(root, k);
        }
    };

    print_rest(root);

    e << EndMap;
    return e.c_str();
}
