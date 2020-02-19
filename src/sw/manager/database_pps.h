// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

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
