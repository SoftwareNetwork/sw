// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <bitset>
#include <map>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "package.h"
//#include "types.h"

namespace sw
{

using ConfigurationPath = Path;
struct ConfigurationValue {};

struct SW_BUILDER_API ConfigurationBase
{
    using PackageConfiguration = std::map<ConfigurationPath, ConfigurationValue>;

    std::string Name;
    std::map<Package, PackageConfiguration> Settings;

    //String getHash() const;
    //String getJson() const;

    ConfigurationBase operator|(const ConfigurationBase &rhs) const;
    ConfigurationBase &operator|=(const ConfigurationBase &rhs);

    void apply(const ConfigurationBase &rhs);
};

/*struct SW_BUILDER_API Configurations
{
    Configurations();

    int32_t registerConfiguration(const Configuration &C);
    Configuration &getConfiguration(ConfigurationType Type);
    const Configuration &getConfiguration(ConfigurationType Type) const;

private:
    std::unordered_map<int32_t, std::unique_ptr<Configuration>> Configs;
    std::unordered_map<std::string, Configuration*> Names;
    mutable std::shared_mutex Mutex;
};*/

SW_BUILDER_API
void addConfigElement(String &c, const String &e);

SW_BUILDER_API
String hashConfig(String &c, bool use_short_config);

}
