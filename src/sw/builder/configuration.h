/*
 * SW - Build System and Package Manager
 * Copyright (C) 2017-2020 Egor Pugin
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <sw/manager/package.h>

#include <bitset>
#include <map>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace sw
{

using ConfigurationPath = Path;
struct ConfigurationValue {};

struct SW_BUILDER_API ConfigurationBase
{
    using PackageConfiguration = std::map<ConfigurationPath, ConfigurationValue>;

    std::string Name;
    std::map<PackageId, PackageConfiguration> Settings;

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

}
