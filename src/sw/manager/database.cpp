// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "database.h"

#include "enums.h"
#include "settings.h"
#include "stamp.h"
#include "storage.h"

#include <sw/support/exceptions.h>
#include <sw/support/hash.h>

#include <primitives/command.h>
#include <primitives/lock.h>
#include <primitives/pack.h>
#include <primitives/templates.h>

// db
#include <primitives/db/sqlite3.h>
#include <sqlite3.h>
#include <sqlpp11/sqlite3/connection.h>
#include <sqlpp11/custom_query.h>
#include <sqlpp11/sqlite3/sqlite3.h>
#include <sqlpp11/sqlpp11.h>
#include <db_service.h>
#include <db_packages.h>

#include <boost/algorithm/string.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <fstream>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "db");

const String service_db_name = "service.db";

namespace sql = sqlpp::sqlite3;

TYPED_EXCEPTION(NoSuchVersion);

#include <inserts.cpp>

namespace sw
{

Database::Database(const path &db_dir, const String &name, const String &schema)
    : db_dir(db_dir)
{
    fn = db_dir / name;

    if (!fs::exists(fn))
    {
        ScopedFileLock lock(fn);
        if (!fs::exists(fn))
        {
            open();
        }
    }

    if (!db)
        open();

    // create or update schema
    fs::create_directories(db_dir);
    primitives::db::sqlite3::SqliteDatabase db2(db->native_handle());
    createOrUpdateSchema(db2, schema, true);
}

Database::~Database() = default;

void Database::open(bool read_only)
{
    sql::connection_config config;
    config.path_to_database = normalize_path(fn);
    config.flags = 0;
    config.flags |= SQLITE_OPEN_FULLMUTEX;// SQLITE_OPEN_NOMUTEX;
    if (read_only)
        config.flags |= SQLITE_OPEN_READONLY;
    else
        config.flags |= SQLITE_OPEN_CREATE | SQLITE_OPEN_READWRITE;
    //config.debug = true;
    db = std::make_unique<sql::connection>(config);

    // prevent SQLITE_BUSY rc
    // hope 1 min is enough to wait for write operation
    // in multithreaded environment
    sqlite3_busy_timeout(db->native_handle(), 60000);

    // explicit
    db->execute("PRAGMA foreign_keys = ON");
}

template <typename T>
std::optional<T> Database::getValue(const String &key) const
{
    return primitives::db::sqlite3::SqliteDatabase(db->native_handle()).getValue<T>(key);
}

template <typename T>
T Database::getValue(const String &key, const T &default_) const
{
    return primitives::db::sqlite3::SqliteDatabase(db->native_handle()).getValue<T>(key, default_);
}

template <typename T>
void Database::setValue(const String &key, const T &v) const
{
    primitives::db::sqlite3::SqliteDatabase(db->native_handle()).setValue(key, v);
}

int Database::getIntValue(const String &key)
{
    return getValue(key, 0);
}

void Database::setIntValue(const String &key, int v)
{
    return setValue(key, v);
}

ServiceDatabase::ServiceDatabase(const path &db_dir)
    : Database(db_dir, service_db_name, service_db_schema)
{
}

void ServiceDatabase::init()
{
    RUN_ONCE
    {
        checkForUpdates();
    };
}

void ServiceDatabase::checkForUpdates() const
{
    using namespace std::literals;

    auto last_check = getLastClientUpdateCheck();
    auto d = Clock::now() - last_check;
    if (d < 3h)
        return;

    try
    {
        // if there are updates, set next check (and notification) in 20 mins
        // to issue a message every run
        /*if (Settings::get_user_settings().checkForUpdates())
            setLastClientUpdateCheck(last_check + 20min);
        else
            setLastClientUpdateCheck();*/
    }
    catch (...)
    {
    }
}

TimePoint ServiceDatabase::getLastClientUpdateCheck() const
{
    return Clock::from_time_t(getValue("NextClientVersionCheck", 0LL));
}

void ServiceDatabase::setLastClientUpdateCheck(const TimePoint &p) const
{
    setValue("NextClientVersionCheck", Clock::to_time_t(p));
}

int ServiceDatabase::getPackagesDbSchemaVersion() const
{
    return getValue("PackagesDbSchemaVersion", 0);
}

void ServiceDatabase::setPackagesDbSchemaVersion(int version) const
{
    setValue("PackagesDbSchemaVersion", version);
}

void ServiceDatabase::clearConfigHashes() const
{
    //db->execute("delete from Configs");
}

String ServiceDatabase::getConfigByHash(const String &settings_hash) const
{
    String c;
    /*db->execute("select config_name from Configs where hash = '" + settings_hash + "'",
                [&c](SQLITE_CALLBACK_ARGS) {
                    c = cols[0];
                    return 0;
                });*/
    return c;
}

void ServiceDatabase::addConfigHash(const String &settings_hash, const String &config, const String &config_hash) const
{
    if (config.empty())
        return;
    //db->execute("replace into Configs values ('" + settings_hash + "', '" + config + "', '" + config_hash + "'" + ")");
}

void ServiceDatabase::removeConfigHashes(const String &h) const
{
    //db->execute("delete from Configs where hash = '" + h + "'");
}

int ServiceDatabase::addConfig(const String &config) const
{
    int id = 0;
    /*db->execute("replace into Configs (name, hash) values ('" + config + "', '" + sha256(config) + "')",
                [&id](SQLITE_CALLBACK_ARGS) {
                    id = std::stoi(cols[0]);
                    return 0;
                });*/
    return id;
}

int ServiceDatabase::getConfig(const String &config) const
{
    int id = 0;
    /*db->execute("select id from Configs where name = '" + config + "'",
                [&id](SQLITE_CALLBACK_ARGS) {
                    id = std::stoi(cols[0]);
                    return 0;
                });
    if (id == 0)
    {
        id = addConfig(config);
        if (id == 0)
            return getConfig(config);
    }*/
    return id;
}

void ServiceDatabase::setPackageDependenciesHash(const PackageId &p, const String &hash) const
{
    //db->execute("replace into PackageDependenciesHashes values ('" + p.toString() + "', '" + hash + "')");
}

bool ServiceDatabase::hasPackageDependenciesHash(const PackageId &p, const String &hash) const
{
    bool has = false;
    /*db->execute("select * from PackageDependenciesHashes where package = '" + p.toString() + "' "
                                                                                              "and dependencies = '" +
                    hash + "'",
                [&has](SQLITE_CALLBACK_ARGS) {
                    has = true;
                    return 0;
                });*/
    return has;
}

void ServiceDatabase::addInstalledPackage(const PackageId &p, PackageVersionGroupNumber group_number) const
{
    SW_UNIMPLEMENTED;

    /*auto h = p.getFilesystemHash();
    if (getInstalledPackageHash(p) == h)
        return;

    const auto ipkgs = ::db::service::InstalledPackage{};
    (*db)(sqlpp::sqlite3::insert_or_replace_into(ipkgs).set(
        ipkgs.path = p.ppath.toString(),
        ipkgs.version = p.version.toString(),
        ipkgs.hash = p.getFilesystemHash(),
        ipkgs.groupNumber = group_number
    ));*/
}

void ServiceDatabase::removeInstalledPackage(const PackageId &p) const
{
    const auto ipkgs = ::db::service::InstalledPackage{};
    (*db)(remove_from(ipkgs).where(ipkgs.path == p.ppath.toString() and ipkgs.version == p.version.toString()));
}

String ServiceDatabase::getInstalledPackageHash(const PackageId &p) const
{
    const auto ipkgs = ::db::service::InstalledPackage{};
    auto q = (*db)(
        custom_query(sqlpp::verbatim("SELECT hash FROM installed_package WHERE path = '" +
            p.ppath.toString() + "'  COLLATE NOCASE AND version = '" + p.version.toString() + "'"))
        .with_result_type_of(select(ipkgs.hash).from(ipkgs))
        );
    //auto q = (*db)(select(ipkgs.hash).from(ipkgs).where(ipkgs.path == p.ppath.toString() and ipkgs.version == p.version.toString()));
    if (q.empty())
        return {};
    return q.front().hash.value();
}

int64_t ServiceDatabase::getInstalledPackageId(const PackageId &p) const
{
    const auto ipkgs = ::db::service::InstalledPackage{};
    auto q = (*db)(
        custom_query(sqlpp::verbatim("SELECT installed_package_id FROM installed_package WHERE path = '" +
            p.ppath.toString() + "' COLLATE NOCASE AND version = '" + p.version.toString() + "'"))
        .with_result_type_of(select(ipkgs.installedPackageId).from(ipkgs))
        );
    //auto q = (*db)(select(ipkgs.installedPackageId).from(ipkgs).where(ipkgs.path == p.ppath.toString() and ipkgs.version == p.version.toString()));
    if (q.empty())
        return 0;
    return q.front().installedPackageId.value();
}

int ServiceDatabase::getInstalledPackageConfigId(const PackageId &p, const String &config) const
{
    auto pid = getInstalledPackageId(p);
    if (pid == 0)
    {
        LOG_DEBUG(logger, "PackageId is not installed: " + p.toString());
        return 0;
    }
    int cid = getConfig(config);
    int id = 0;
    /*db->execute("select PackageProperties.id from PackageProperties "
                "where package_id = '" +
                    std::to_string(pid) + "' "
                                          "and config_id = '" +
                    std::to_string(cid) + "'",
                [&id](SQLITE_CALLBACK_ARGS) {
                    id = std::stoi(cols[0]);
                    return 0;
                });
    if (id == 0)
    {
        db->execute("insert into PackageProperties (package_id, config_id) values ("
                    "'" +
                        std::to_string(pid) + "', "
                                              "'" +
                        std::to_string(cid) + "')",
                    [](SQLITE_CALLBACK_ARGS) { return 0; }, true);
        return getInstalledPackageConfigId(p, config);
    }*/
    return id;
}

SomeFlags ServiceDatabase::getInstalledPackageFlags(const PackageId &p, const String &config) const
{
    SomeFlags f = 0;
    /*db->execute("select flags from PackageProperties "
                "where id = '" +
                    std::to_string(getInstalledPackageConfigId(p, config)) + "'",
                [&f](SQLITE_CALLBACK_ARGS) {
                    f = std::stoull(cols[0]);
                    return 0;
                });*/
    return f;
}

void ServiceDatabase::setInstalledPackageFlags(const PackageId &p, const String &config, const SomeFlags &f) const
{
    /*db->execute("update PackageProperties set flags = '" + std::to_string(f.to_ullong()) +
                "' where id = '" + std::to_string(getInstalledPackageConfigId(p, config)) + "'");*/
}

std::optional<ServiceDatabase::OverriddenPackage> ServiceDatabase::getOverriddenPackage(const PackageId &pkg) const
{
    const auto &pkgs = getOverriddenPackages();
    auto i = pkgs.find(pkg);
    if (i == pkgs.end(pkg))
        return {};
    return i->second;
}

const ServiceDatabase::OverriddenPackages &ServiceDatabase::getOverriddenPackages() const
{
    // maybe move them to packages db?
    if (override_remote_packages)
        return override_remote_packages.value();

    OverriddenPackages pkgs;
    const auto orp = ::db::service::OverrideRemotePackage{};
    const auto orpv = ::db::service::OverrideRemotePackageVersion{};
    const auto orpvd = ::db::service::OverrideRemotePackageVersionDependency{};
    for (const auto &row : (*db)(select(orp.overrideRemotePackageId, orp.path).from(orp).unconditionally()))
    {
        for (const auto &row2 : (*db)(select(orpv.overrideRemotePackageVersionId, orpv.version, orpv.sdir, orpv.prefix).from(orpv).where(orpv.overrideRemotePackageId == row.overrideRemotePackageId)))
        {
            auto &o = pkgs[PackagePath(row.path.value())][row2.version.value()];
            o.id = -row2.overrideRemotePackageVersionId.value();
            o.sdir = row2.sdir.value();
            o.prefix = row2.prefix.value();
            for (const auto &row3 : (*db)(select(orpvd.dependency).from(orpvd).where(orpvd.overrideRemotePackageVersionId == row2.overrideRemotePackageVersionId)))
            {
                o.deps.insert(row3.dependency.value());
            }
        }
    }
    override_remote_packages = pkgs;
    return override_remote_packages.value();
}

void ServiceDatabase::overridePackage(const PackageId &pkg, const OverriddenPackage &opkg) const
{
    getOverriddenPackages(); // init if needed

    override_remote_packages.value().erase(pkg);
    override_remote_packages.value().emplace(pkg, opkg);

    const auto orp = ::db::service::OverrideRemotePackage{};
    const auto orpv = ::db::service::OverrideRemotePackageVersion{};
    const auto orpvd = ::db::service::OverrideRemotePackageVersionDependency{};
    db->start_transaction();
    deleteOverriddenPackage(pkg);
    auto q1 = (*db)(select(orp.overrideRemotePackageId).from(orp).where(
        orp.path == pkg.ppath.toString()
    ));
    if (q1.empty())
    {
        (*db)(insert_into(orp).set(
            orp.path = pkg.ppath.toString()
        ));
    }
    auto q = (*db)(select(orp.overrideRemotePackageId).from(orp).where(
        orp.path == pkg.ppath.toString()
    ));
    (*db)(insert_into(orpv).set(
        orpv.overrideRemotePackageId = q.front().overrideRemotePackageId.value(),
        orpv.version = pkg.version.toString(),
        orpv.sdir = fs::canonical(opkg.sdir).u8string(),
        orpv.prefix = opkg.prefix
    ));
    auto q2 = (*db)(select(orpv.overrideRemotePackageVersionId).from(orpv).where(
        orpv.overrideRemotePackageId == q.front().overrideRemotePackageId.value() &&
        orpv.version == pkg.version.toString()
    ));
    for (auto &d : opkg.deps)
    {
        (*db)(insert_into(orpvd).set(
            orpvd.overrideRemotePackageVersionId = q2.front().overrideRemotePackageVersionId.value(),
            orpvd.dependency = d.toString()
        ));
    }
    db->commit_transaction();
}

void ServiceDatabase::deleteOverriddenPackage(const PackageId &pkg) const
{
    const auto orp = ::db::service::OverrideRemotePackage{};
    const auto orpv = ::db::service::OverrideRemotePackageVersion{};
    auto q = (*db)(select(orp.overrideRemotePackageId).from(orp).where(
        orp.path == pkg.ppath.toString()
    ));
    if (q.empty())
        return;
    (*db)(remove_from(orpv).where(
        orpv.overrideRemotePackageId == q.front().overrideRemotePackageId.value() &&
        orpv.version == pkg.version.toString()
    ));
}

void ServiceDatabase::deleteOverriddenPackageDir(const path &sdir) const
{
    const auto orpv = ::db::service::OverrideRemotePackageVersion{};
    (*db)(remove_from(orpv).where(
        orpv.sdir == fs::canonical(sdir).u8string()
    ));
}

UnresolvedPackages ServiceDatabase::getOverriddenPackageVersionDependencies(db::PackageVersionId project_version_id)
{
    //project_version_id = abs(project_version_id);
    const auto orpvd = ::db::service::OverrideRemotePackageVersionDependency{};
    UnresolvedPackages deps;
    for (const auto &row3 : (*db)(select(orpvd.dependency).from(orpvd).where(orpvd.overrideRemotePackageVersionId == project_version_id)))
        deps.insert(row3.dependency.value());
    return deps;
}

Packages ServiceDatabase::getInstalledPackages() const
{
    const auto ipkgs = ::db::service::InstalledPackage{};
    Packages pkgs;
    for (const auto &row : (*db)(select(ipkgs.path, ipkgs.version).from(ipkgs).unconditionally()))
    {
        SW_UNIMPLEMENTED;

        /*Package pkg;
        pkg.ppath = row.path.value();
        pkg.version = row.version.value();
        //pkg.createNames();
        pkgs.insert(pkg);*/
    }
    return pkgs;
}

PackagesDatabase::PackagesDatabase(const path &db_fn)
    : Database(db_fn.parent_path(), db_fn.filename().string(), packages_db_schema)
{
}

std::unordered_map<UnresolvedPackage, PackageId> PackagesDatabase::resolve(const UnresolvedPackages &in_pkgs, UnresolvedPackages &unresolved_pkgs) const
{
    const auto pkgs = ::db::packages::Package{};

    std::unordered_map<UnresolvedPackage, PackageId> r;
    for (auto &pkg : in_pkgs)
    {
        auto pid = getPackageId(pkg.ppath);
        if (!pid)
        {
            unresolved_pkgs.insert(pkg);
            continue;
        }

        db::PackageVersionId id = 0;
        VersionSet versions;
        UnorderedVersionMap<db::PackageVersionId> version_ids;

        const auto pkg_ver = ::db::packages::PackageVersion{};
        for (const auto &row : (*db)(
            select(pkg_ver.packageVersionId, pkg_ver.version)
            .from(pkg_ver)
            .where(pkg_ver.packageId == pid)))
        {
            versions.insert(row.version.value());
            version_ids[row.version.value()] = row.packageVersionId.value();
        }

        auto v = pkg.range.getMaxSatisfyingVersion(versions);
        if (!v)
        {
            unresolved_pkgs.insert(pkg);
            continue;
        }

        r.emplace(pkg, PackageId{ pkg.ppath, *v });
    }
    return r;
}

PackageData PackagesDatabase::getPackageData(const PackageId &p) const
{
    const auto pkgs = ::db::packages::Package{};
    const auto pkg_ver = ::db::packages::PackageVersion{};
    const auto pkgdeps = ::db::packages::PackageVersionDependency{};

    PackageData d;

    auto q = (*db)(
        select(pkg_ver.packageVersionId, pkg_ver.hash, pkg_ver.flags, pkg_ver.updated, pkg_ver.groupNumber, pkg_ver.prefix)
        .from(pkg_ver)
        .where(pkg_ver.packageId == getPackageId(p.ppath) && pkg_ver.version == p.version.toString()));
    if (q.empty())
        throw SW_RUNTIME_ERROR("No such package in db: " + p.toString());
    auto &row = q.front();
    d.hash = row.hash.value();
    d.flags = row.flags.value();
    d.group_number = row.groupNumber.value();
    d.prefix = (int)row.prefix.value();

    for (const auto &row : (*db)(
        select(pkgs.packageId, pkgs.path, pkgdeps.versionRange)
        .from(pkgdeps.join(pkgs).on(pkgdeps.packageId == pkgs.packageId))
        .where(pkgdeps.packageVersionId == row.packageVersionId)))
    {
        d.dependencies.emplace(row.path.value(), row.versionRange.value());
    }

    return d;
}

int64_t PackagesDatabase::getInstalledPackageId(const PackageId &p) const
{
    const auto pkgs = ::db::packages::Package{};
    const auto pkg_ver = ::db::packages::PackageVersion{};

    auto q = (*db)(
        select(pkg_ver.packageVersionId, pkg_ver.hash, pkg_ver.flags, pkg_ver.updated, pkg_ver.groupNumber, pkg_ver.prefix)
        .from(pkg_ver)
        .where(pkg_ver.packageId == getPackageId(p.ppath) && pkg_ver.version == p.version.toString()));
    if (q.empty())
        return 0;
    return q.front().packageVersionId.value();
}

String PackagesDatabase::getInstalledPackageHash(const PackageId &p) const
{
    const auto pkgs = ::db::packages::Package{};
    const auto pkg_ver = ::db::packages::PackageVersion{};

    auto q = (*db)(
        select(pkg_ver.packageVersionId, pkg_ver.hash, pkg_ver.flags, pkg_ver.updated, pkg_ver.groupNumber, pkg_ver.prefix)
        .from(pkg_ver)
        .where(pkg_ver.packageId == getPackageId(p.ppath) && pkg_ver.version == p.version.toString()));
    if (q.empty())
        return {};
    return q.front().hash.value();
}

bool PackagesDatabase::isPackageInstalled(const Package &p) const
{
    return getInstalledPackageId(p) != 0 && getInstalledPackageHash(p) == p.getData().hash;
}

void PackagesDatabase::installPackage(const Package &p)
{
    const auto pkg = ::db::packages::Package{};
    const auto pkgv = ::db::packages::PackageVersion{};
    const auto pkgvd = ::db::packages::PackageVersionDependency{};

    std::lock_guard lk(m);

    db->start_transaction();

    int64_t package_id = 0;

    // get package id
    auto q = (*db)(select(pkg.packageId).from(pkg).where(
        pkg.path == p.ppath.toString()
        ));
    if (q.empty())
    {
        // add package
        (*db)(insert_into(pkg).set(
            pkg.path = p.ppath.toString()
        ));

        // get package id
        auto q = (*db)(select(pkg.packageId).from(pkg).where(
            pkg.path == p.ppath.toString()
            ));
        package_id = q.front().packageId.value();
    }
    else
    {
        package_id = q.front().packageId.value();

        // remove existing version
        (*db)(remove_from(pkgv).where(
            pkgv.packageId == package_id &&
            pkgv.version == p.version.toString()
            ));
    }

    PackageVersionGroupNumber gn;
    auto i = storage_gns.find({ p.storage.getName(), p.getData().group_number });
    if (i == storage_gns.end())
        gn = storage_gns[{ p.storage.getName(), p.getData().group_number }] = getMaxGroupNumber() + 1;
    else
        gn = storage_gns[{ p.storage.getName(), p.getData().group_number }];

    // insert version
    (*db)(insert_into(pkgv).set(
        // basic data
        pkgv.packageId = package_id,
        pkgv.version = p.version.toString(),

        // extended
        pkgv.prefix = p.getData().prefix,
        pkgv.hash = p.getData().hash,
        pkgv.groupNumber = gn,

        // TODO:
        pkgv.archiveVersion = 1,

        // misc
        pkgv.updated = ""

        //pkgv.sdir = fs::canonical(opkg.sdir).u8string(),
    ));

    // get version id
    auto q2 = (*db)(select(pkgv.packageVersionId).from(pkgv).where(
        pkgv.packageId == package_id &&
        pkgv.version == p.version.toString()
        ));
    for (auto &d : p.getData().dependencies)
    {
        // get package id
        auto q = (*db)(select(pkg.packageId).from(pkg).where(
            pkg.path == d.ppath.toString()
            ));
        if (q.empty())
        {
            // add package
            (*db)(insert_into(pkg).set(
                pkg.path = d.ppath.toString()
            ));

            // get package id
            q = (*db)(select(pkg.packageId).from(pkg).where(
                pkg.path == d.ppath.toString()
                ));
        }

        // insert deps
        (*db)(insert_into(pkgvd).set(
            pkgvd.packageVersionId = q2.front().packageVersionId.value(),
            pkgvd.packageId = q.front().packageId.value(),
            pkgvd.versionRange = d.range.toString()
        ));
    }

    db->commit_transaction();
}

PackageVersionGroupNumber PackagesDatabase::getMaxGroupNumber() const
{
    const auto pkg_ver = ::db::packages::PackageVersion{};

    auto q = (*db)(
        select(max(pkg_ver.groupNumber))
        .from(pkg_ver)
        .unconditionally());
    if (q.empty())
        return {};
    return q.front().max.value();
}

std::optional<path> PackagesDatabase::getOverriddenDir(const Package &p) const
{
    const auto pkgs = ::db::packages::Package{};
    const auto pkg_ver = ::db::packages::PackageVersion{};

    auto q = (*db)(
        select(pkg_ver.sdir)
        .from(pkg_ver)
        .where(pkg_ver.packageId == getPackageId(p.ppath) && pkg_ver.version == p.version.toString()));
    if (q.empty() || q.front().sdir.is_null())
        return {};
    return q.front().sdir.value();
}

/*void PackagesDatabase::findLocalDependencies(IdDependencies &id_deps, const UnresolvedPackages &deps) const
{
    SW_UNIMPLEMENTED;

    /*preInitFindDependencies();

    std::map<PackageId, DownloadDependency> all_deps;
    for (auto &[id, dep] : id_deps)
        all_deps.emplace(dep, dep);

    for (auto &dep : deps)
    {
        auto &pkgs = ls.getServiceDatabase().getOverriddenPackages();
        PackageId pkg{ dep.ppath, dep.range.toString() };
        auto i = pkgs.find(pkg);
        if (i != pkgs.end(pkg))
        {
            SW_UNIMPLEMENTED;

            /*DownloadDependency project;
            project.id = i->second.id;
            //project.flags.set(pfDirectDependency);
            project.ppath = dep.ppath;
            project.version = i->first.version;
            project.prefix = i->second.prefix;
            project.db_dependencies = getProjectDependencies(project.id, all_deps);
            //project.setDependencyIds(getProjectDependencies(project.id, all_deps, i->second.deps)); // see dependency.h note
            all_deps[project] = project; // assign first, deps assign second
            continue;
        }

        // TODO: replace later with typed exception, so client will try to fetch same package from server
        throw SW_RUNTIME_ERROR("PackageId '" + dep.ppath.toString() + "' not found.");
    }

    // mark local deps
    const auto &overridden = ls.getServiceDatabase().getOverriddenPackages();
    if (!overridden.empty())
    {
        for (auto &[pkg, d] : all_deps)
            d.local_override = overridden.find(pkg) != overridden.end(pkg);
    }

    // make id deps
    for (auto &ad : all_deps)
    {
        SW_UNIMPLEMENTED;

        /*auto &d = ad.second;
        std::unordered_set<db::PackageVersionId> ids;
        for (auto &dd2 : d.db_dependencies) // see dependency.h note
            ids.insert(dd2.second.id);
        d.setDependencyIds(ids);
        id_deps[d.id] = d;
    }
}

IdDependencies PackagesDatabase::findDependencies(const UnresolvedPackages &deps) const
{
    SW_UNIMPLEMENTED;

    /*const auto pkgs = ::db::packages::Package{};

    preInitFindDependencies();

    const auto &overridden = ls.getServiceDatabase().getOverriddenPackages();

    DependenciesMap all_deps;
    for (auto &dep : deps)
    {
        if (dep.ppath.is_loc())
            continue;

        DownloadDependency project;
        project.ppath = dep.ppath;
        project.range = dep.range;
        auto q = (*db)(
            custom_query(sqlpp::verbatim("SELECT package_id FROM package WHERE path = '" + dep.ppath.toString() + "' COLLATE NOCASE"))
            .with_result_type_of(select(pkgs.packageId).from(pkgs))
            );
        if (q.empty())
        {
            auto &pkgs = overridden;
            auto i = pkgs.find(dep.ppath);
            if (i != pkgs.end(dep.ppath))
            {
                //throw SW_RUNTIME_ERROR("Not imlpemented");
                //project.id = i->second.id;
                project.id = getExactProjectVersionId(project, project.version, project.flags, project.hash, project.group_number, project.prefix);
                //project.flags.set(pfDirectDependency);
                all_deps[project] = project; // assign first, deps assign second
                project.db_dependencies = getProjectDependencies(project.id, all_deps);
                //project.setDependencyIds(getProjectDependencies(project.id, all_deps, i->second.deps)); // see dependency.h note
                continue;
            }

            // TODO: replace later with typed exception, so client will try to fetch same package from server
            throw SW_RUNTIME_ERROR("PackageId '" + project.ppath.toString() + "' not found.");
        }

        project.id = q.front().packageId.value(); // set package id first, then it is replaced with pkg version id, do not remove
        project.id = getExactProjectVersionId(project, project.version, project.flags, project.hash, project.group_number, project.prefix);
        //project.flags.set(pfDirectDependency);
        all_deps[project] = project; // assign first, deps assign second
        all_deps[project].db_dependencies = getProjectDependencies(project.id, all_deps);
        //all_deps[project].setDependencyIds(getProjectDependencies(project.id, all_deps)); // see dependency.h note
    }

    // mark local deps
    if (!overridden.empty())
    {
        for (auto &[pkg, d] : all_deps)
            d.local_override = overridden.find(pkg) != overridden.end(pkg);
    }

    // make id deps
    IdDependencies dds;
    for (auto &ad : all_deps)
    {
        SW_UNIMPLEMENTED;
        /*auto &d = ad.second;
        std::unordered_set<db::PackageVersionId> ids;
        for (auto &dd2 : d.db_dependencies) // see dependency.h note
            ids.insert(dd2.second.id);
        d.setDependencyIds(ids);
        dds[d.id] = d;
    }
    return dds;
}

static void check_version_age(const std::string &created)
{
    auto d = tstart - string2timepoint(created);
    auto mins = std::chrono::duration_cast<std::chrono::minutes>(d).count();
    // multiple by 2 because first time interval goes for uploading db
    // and during the second one, the packet is really young
    //LOG_INFO(logger, "mins " << mins);
    if (mins < PACKAGES_DB_REFRESH_TIME_MINUTES * 2)
        throw std::runtime_error("One of the queried packages is 'young'. Young packages must be retrieved from server.");
}

db::PackageVersionId PackagesDatabase::getExactProjectVersionId(const DownloadDependency &project, Version &version, SomeFlags &flags, String &hash, PackageVersionGroupNumber &gn, int &prefix) const
{
    SW_UNIMPLEMENTED;

    /*auto err = [](const auto &p, const auto &r)
    {
        return NoSuchVersion("No suitable version '" + r.toString() + "' for project '" + p.toString() + "'");
    };

    db::PackageVersionId id = 0;
    VersionSet versions;
    UnorderedVersionMap<db::PackageVersionId> version_ids;

    const auto pkg_ver = ::db::packages::PackageVersion{};
    for (const auto &row : (*db)(
        select(pkg_ver.packageVersionId, pkg_ver.version)
        .from(pkg_ver)
        .where(pkg_ver.packageId == project.id)))
    {
        versions.insert(row.version.value());
        version_ids[row.version.value()] = row.packageVersionId.value();
    }

    // also check local db
    auto &o = ls.getServiceDatabase().getOverriddenPackages();
    auto i = o.find(project.ppath);
    if (i != o.end(project.ppath))
    {
        for (auto &[v, d] : i->second)
        {
            versions.insert(v);
            version_ids[v] = d.id;
        }
    }

    auto v = project.range.getMaxSatisfyingVersion(versions);
    if (!v)
        throw err(project.ppath, project.range);

    PackageId pkg{ project.ppath, v.value() };
    auto i2 = o.find(pkg);
    if (i2 != o.end(pkg))
    {
        id = version_ids[v.value()];
        version = v.value();
        prefix = i2->second.prefix;
        gn = i2->second.getGroupNumber();
        return id;
    }

    id = version_ids[v.value()];
    auto q = (*db)(
        select(pkg_ver.hash, pkg_ver.flags, pkg_ver.updated, pkg_ver.groupNumber, pkg_ver.prefix)
        .from(pkg_ver)
        .where(pkg_ver.packageVersionId == id));
    auto &row = q.front();
    version = v.value();
    hash = row.hash.value();
    flags = row.flags.value();
    check_version_age(row.updated.value());
    gn = row.groupNumber.value();
    prefix = (int)row.prefix.value();

    return id;
}

PackagesDatabase::Dependencies PackagesDatabase::getProjectDependencies(db::PackageVersionId project_version_id, DependenciesMap &dm) const
{
    SW_UNIMPLEMENTED;

    /*const auto pkgs = ::db::packages::Package{};
    const auto pkgdeps = ::db::packages::PackageVersionDependency{};

    Dependencies dependencies;
    std::vector<DownloadDependency> deps;

    if (project_version_id > 0)
    {
        for (const auto &row : (*db)(
            select(pkgs.packageId, pkgs.path, pkgdeps.versionRange)
            .from(pkgdeps.join(pkgs).on(pkgdeps.packageId == pkgs.packageId))
            .where(pkgdeps.packageVersionId == project_version_id)))
        {
            SW_UNIMPLEMENTED;

            /*DownloadDependency dependency;
            dependency.id = row.packageId.value();
            dependency.ppath = row.path.value();
            dependency.range = row.versionRange.value();
            dependency.id = getExactProjectVersionId(dependency, dependency.version, dependency.flags, dependency.hash, dependency.group_number, dependency.prefix);
            auto i = dm.find(dependency);
            if (i == dm.end())
            {
                dm[dependency] = dependency; // assign first, deps assign second
                dm[dependency].db_dependencies = getProjectDependencies(dependency.id, dm);
                //dm[dependency].setDependencyIds(getProjectDependencies(dependency.id, dm)); // see dependency.h note
            }
            dependencies[dependency.ppath.toString()] = dependency; // see dependency.h note
        }
    }
    else if (project_version_id < 0) // overridden
    {
        const auto &overridden_deps = ls.getServiceDatabase().getOverriddenPackageVersionDependencies(-project_version_id);
        for (const auto &d : overridden_deps)
        {
            SW_UNIMPLEMENTED;
            /*DownloadDependency dependency;
            dependency.id = -1;

            // read pkg id
            auto q = (*db)(
                custom_query(sqlpp::verbatim("SELECT package_id FROM package WHERE path = '" + d.ppath.toString() + "' COLLATE NOCASE"))
                .with_result_type_of(select(pkgs.packageId).from(pkgs))
                );
            if (!q.empty())
                dependency.id = q.front().packageId.value();
            //else // not registered yet, remote or local-overridden
            // left as is

            dependency.ppath = d.ppath;
            dependency.range = d.range;
            dependency.id = getExactProjectVersionId(dependency, dependency.version, dependency.flags, dependency.hash, dependency.group_number, dependency.prefix);
            auto i = dm.find(dependency);
            if (i == dm.end())
            {
                dm[dependency] = dependency; // assign first, deps assign second
                dm[dependency].db_dependencies = getProjectDependencies(dependency.id, dm);
                //dm[dependency].setDependencyIds(getProjectDependencies(dependency.id, dm)); // see dependency.h note
            }
            dependencies[dependency.ppath.toString()] = dependency; // see dependency.h note
        }
    }
    return dependencies;
}*/

void PackagesDatabase::listPackages(const String &name) const
{
    auto pkgs = getMatchingPackages<std::set>(name);
    if (pkgs.empty())
    {
        LOG_INFO(logger, "nothing found");
        return;
    }

    for (auto &pkg : pkgs)
    {
        auto v1 = getVersionsForPackage(pkg);
        VersionSet versions(v1.begin(), v1.end());
        String out = pkg.toString();
        out += " (";
        for (auto &v : versions)
            out += v.toString() + ", ";
        out.resize(out.size() - 2);
        out += ")";
        LOG_INFO(logger, out);
    }
}

Version PackagesDatabase::getExactVersionForPackage(const PackageId &p) const
{
    Version v = p.version;
    /*DownloadDependency d;
    d.ppath = p.ppath;
    d.id = getPackageId(p.ppath);

    SomeFlags f;
    String h;
    PackageVersionGroupNumber gn;
    int prefix;
    getExactProjectVersionId(d, v, f, h, gn, prefix);*/
    return v;
}

template <template <class...> class C>
C<PackagePath> PackagesDatabase::getMatchingPackages(const String &name) const
{
    const auto tpkgs = ::db::packages::Package{};

    C<PackagePath> pkgs;
    String q;
    if (name.empty())
        q = "SELECT path FROM package ORDER BY path COLLATE NOCASE";
    else
        q = "SELECT path FROM package WHERE path like '%" + name + "%' ORDER BY path COLLATE NOCASE";
    for (const auto &row : (*db)(
        custom_query(sqlpp::verbatim(q))
        .with_result_type_of(select(tpkgs.path).from(tpkgs))
        ))
    {
        pkgs.insert(row.path.value());
    }
    return pkgs;
}

template std::unordered_set<PackagePath>
PackagesDatabase::getMatchingPackages<std::unordered_set>(const String &) const;

template std::set<PackagePath>
PackagesDatabase::getMatchingPackages<std::set>(const String &) const;

std::vector<Version> PackagesDatabase::getVersionsForPackage(const PackagePath &ppath) const
{
    std::vector<Version> versions;
    const auto vpkgs = ::db::packages::PackageVersion{};
    for (const auto &row : (*db)(select(vpkgs.version).from(vpkgs).where(vpkgs.packageId == getPackageId(ppath))))
        versions.push_back(row.version.value());
    return versions;
}

db::PackageId PackagesDatabase::getPackageId(const PackagePath &ppath) const
{
    const auto pkgs = ::db::packages::Package{};
    auto q = (*db)(
        custom_query(sqlpp::verbatim("SELECT package_id FROM package WHERE path = '" + ppath.toString() + "' COLLATE NOCASE"))
        .with_result_type_of(select(pkgs.packageId).from(pkgs))
        );
    if (q.empty())
        return 0;
    return q.front().packageId.value();
}

PackageId PackagesDatabase::getGroupLeader(PackageVersionGroupNumber n) const
{
    const auto pkgs = ::db::packages::Package{};
    const auto vpkgs = ::db::packages::PackageVersion{};
    for (const auto &row : (*db)(select(pkgs.path, vpkgs.version)
        .from(vpkgs.join(pkgs).on(vpkgs.packageId == pkgs.packageId))
        .where(vpkgs.groupNumber == n)
        .order_by(vpkgs.groupNumber.asc()))
        )
    {
        return { row.path.value(), row.version.value() };
    }
    throw SW_RUNTIME_ERROR("Group leader not found for group: " + std::to_string(n));
}

Packages PackagesDatabase::getDependentPackages(const PackageId &pkg)
{
    Packages r;

    // 1. Find current project version id.
    auto project_id = getPackageId(pkg.ppath);

    // 2. Find project versions dependent on this version.
    std::set<std::pair<String, String>> pkgs_s;
    /*db->execute(
        "select path, case when branch is not null then branch else major || '.' || minor || '.' || patch end as version "
        "from package_version_dependency "
        "join package_version on package_version.id = project_version_id "
        "join package on package.id = project_id "
        "where project_dependency_id = '" +
            std::to_string(project_id) + "'",
        [&pkgs_s](SQLITE_CALLBACK_ARGS) {
            pkgs_s.insert({cols[0], cols[1]});
            return 0;
        });*/

    for (auto &p : pkgs_s)
    {
        SW_UNIMPLEMENTED;

        /*Package pkg;
        pkg.ppath = p.first;
        pkg.version = p.second;
        //pkg.createNames();
        r.insert(pkg);*/
    }

    return r;
}

Packages PackagesDatabase::getDependentPackages(const Packages &pkgs)
{
    Packages r;
    for (auto &pkg : pkgs)
    {
        auto dpkgs = getDependentPackages(pkg);
        r.merge(dpkgs);
    }

    // exclude input
    for (auto &pkg : pkgs)
        r.erase(pkg);

    return r;
}

Packages PackagesDatabase::getTransitiveDependentPackages(const Packages &pkgs)
{
    SW_UNIMPLEMENTED;

    /*auto r = pkgs;
    std::map<PackageId, bool> retrieved;
    while (1)
    {
        bool changed = false;

        for (auto &pkg : r)
        {
            if (retrieved[pkg])
                continue;

            retrieved[pkg] = true;
            changed = true;

            auto dpkgs = getDependentPackages(pkg);
            r.merge(dpkgs);
            break;
        }

        if (!changed)
            break;
    }

    // exclude input
    for (auto &pkg : pkgs)
        r.erase(pkg);

    return r;*/
}

DataSources PackagesDatabase::getDataSources() const
{
    const auto ds = ::db::packages::DataSource{};

    DataSources dss;
    for (const auto &row : (*db)(select(ds.url, ds.flags).from(ds).unconditionally()))
    {
        DataSource s;
        s.raw_url = row.url;
        s.flags = row.flags.value();
        if (s.flags[DataSource::fDisabled])
            continue;
        dss.push_back(s);
    }
    if (dss.empty())
        throw SW_RUNTIME_ERROR("No data sources available");
    return dss;
}

/*std::optional<ExtendedPackageData> PackagesDatabase::getPackageInformation(const PackageId &p) const
{
    const auto pkg_ver = ::db::packages::PackageVersion{};

    SW_UNIMPLEMENTED;

    /*ExtendedPackageData d;
    d.ppath = p.ppath;
    d.version = p.version;

    auto q = (*db)(
        select(pkg_ver.hash, pkg_ver.flags, pkg_ver.updated, pkg_ver.groupNumber, pkg_ver.prefix)
        .from(pkg_ver)
        .where(pkg_ver.packageId == getPackageId(p.ppath) && pkg_ver.version == d.version.toString()));
    if (q.empty())
        return {};
        //throw SW_RUNTIME_ERROR("No such package in local db: " + p.toString());
    auto &row = q.front();
    d.hash = row.hash.value();
    d.flags = row.flags.value();
    d.group_number = row.groupNumber.value();
    d.prefix = (int)row.prefix.value();

    return d;
}*/

}
