/*
 * SW - Build System and Package Manager
 * Copyright (C) 2016-2019 Egor Pugin
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

namespace
{

const auto pkgs = ::db::packages::Package{};
const auto pkg_ver = ::db::packages::PackageVersion{};
const auto pkg_deps = ::db::packages::PackageVersionDependency{};
const auto ds = ::db::packages::DataSource{};

template <typename SelectType>
using PreparedStatement = decltype(((sql::connection*)nullptr)->prepare(*((SelectType*)nullptr)));

auto selectPackageVersionData = []()
{
    return
        select(pkg_ver.packageVersionId, pkg_ver.hash, pkg_ver.flags, pkg_ver.prefix, pkg_ver.sdir)
        .from(pkg_ver)
        .where(pkg_ver.packageId == parameter(pkg_ver.packageId) && pkg_ver.version == parameter(pkg_ver.version));
};

}

namespace sw
{

struct PreparedStatements
{
    PreparedStatement<decltype(selectPackageVersionData())> packageVersionData;

    PreparedStatements(sql::connection &db)
        : packageVersionData(db.prepare(selectPackageVersionData()))
    {
    }
};

}
