// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>

#pragma once

namespace
{

const auto pkgs = ::db::packages::Package{};
const auto pkg_ver = ::db::packages::PackageVersion{};
const auto pkg_deps = ::db::packages::PackageVersionDependency{};
const auto configs = ::db::packages::Config{};
const auto t_files = ::db::packages::File{};
const auto t_pkg_ver_files = ::db::packages::PackageVersionFile{};

template <typename SelectType>
using PreparedStatement = decltype(((sql::connection*)nullptr)->prepare(*((SelectType*)nullptr)));

auto selectPackageVersionData = []()
{
    return
        select(pkg_ver.packageVersionId, pkg_ver.flags, pkg_ver.prefix, pkg_ver.sdir)
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
