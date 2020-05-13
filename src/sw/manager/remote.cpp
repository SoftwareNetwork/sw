/*
 * SW - Build System and Package Manager
 * Copyright (C) 2016-2018 Egor Pugin
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

#include "remote.h"

#include "api.h"
#include "api_protobuf.h"
#include "package.h"

#include <sw/support/hash.h>
#include <sw/support/storage.h>

#include <boost/dll.hpp>
#include <fmt/ostream.h>
#include <nlohmann/json.hpp>
#include <primitives/http.h>
#include <primitives/templates.h>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "remote");

#define SPECIFICATIONS_FILENAME "specification.json"

namespace sw
{

std::vector<std::shared_ptr<Remote>> get_default_remotes()
{
    static std::vector<std::shared_ptr<Remote>> rms;
    RUN_ONCE
    {
        auto r = std::make_shared<Remote>(DEFAULT_REMOTE_NAME, "https://software-network.org/");
        rms.push_back(std::move(r));
    };
    return rms;
}

String DataSource::getUrl(const Package &pkg) const
{
    // {PHPF} = package hash path full
    // {PH64} = package hash, length = 64
    // {FN} = archive name
    return fmt::format(raw_url,
        fmt::arg("PHPF", normalize_path(pkg.getHashPath())),
        fmt::arg("PH64", pkg.getHash().substr(0, 64)),
        fmt::arg("FN", make_archive_name())
    );
}

bool DataSource::downloadPackage(const Package &d, const path &fn, String &dl_hash) const
{
    auto download_from_source = [&](const auto &url)
    {
        try
        {
            LOG_TRACE(logger, "Downloading file: " << url);
            download_file(url, fn);
        }
        catch (std::exception &e)
        {
            LOG_TRACE(logger, "Downloading file: " << url << ", error: " << e.what());
            return false;
        }
        return true;
    };

    auto url = getUrl(d);
    if (download_from_source(url))
    {
        auto sfh = get_strong_file_hash(fn, d.getData().getHash(StorageFileType::SourceArchive));
        if (sfh == d.getData().getHash(StorageFileType::SourceArchive))
        {
            dl_hash = sfh;
            return true;
        }
        auto fh = get_file_hash(fn);
        if (fh == d.getData().getHash(StorageFileType::SourceArchive))
        {
            dl_hash = fh;
            return true;
        }
        LOG_TRACE(logger, "Downloaded file: " << url << " hash = " << sfh);
    }
    return false;
}

Remote::Remote(const String &name, const String &u)
    : name(name), url(u)
{
    path up = url;
    //if (up.filename() != SPECIFICATIONS_FILENAME)
    {
        if (!url.empty() && url.back() != '/')
            url += "/";
    }
    String spec_url = url + "static/" SPECIFICATIONS_FILENAME;
    auto fn = get_root_directory() / "remotes" / name / SPECIFICATIONS_FILENAME;
    if (!fs::exists(fn))
        download_file(spec_url, fn);
    auto j = nlohmann::json::parse(read_file(fn));
    auto &spec = j["specification"];
    api_url = spec["api_url"];
    auto &jdb = spec["database"];
    if (jdb.contains("url"))
        db.url = jdb["url"];
    if (jdb.contains("git_url"))
        db.git_repo_url = jdb["git_url"];
    if (jdb.contains("local_dir"))
        db.local_dir = jdb["local_dir"];
    db.version_root_url = jdb["version_root_url"];
    if (!db.version_root_url.empty() && db.version_root_url.back() != '/')
        db.version_root_url += "/";

    auto &ds = spec["data_sources"];
    for (const auto &row : ds)
    {
        for (auto &[k, v] : row.items())
        {
            DataSource s;
            s.raw_url = v["url"];
            if (v.contains("flags"))
                s.flags = v["flags"].get<int64_t>();
            if (s.flags[DataSource::fDisabled])
                continue;
            dss.push_back(s);
        }
    }
    if (dss.empty())
        throw SW_RUNTIME_ERROR("No data sources available");
}

std::unique_ptr<Api> Remote::getApi() const
{
    switch (getApiType())
    {
    case ApiType::Protobuf:
        return std::make_unique<ProtobufApi>(*this);
    default:
        SW_UNIMPLEMENTED;
    }
}

GrpcChannel Remote::getGrpcChannel() const
{
    // keeping channel for too long causes issues
    // so we create a new one every time

    static std::mutex m;
    std::unique_lock lk(m);

    static const grpc::SslCredentialsOptions ssl_options = []()
    {
        grpc::SslCredentialsOptions ssl_options;
        path certsfn;
        // system's certs first?
        if (auto f = primitives::http::getCaCertificatesBundleFileName())
            certsfn = *f;
        else
        {
            certsfn = sw::get_ca_certs_filename();
            if (!fs::exists(certsfn))
                throw SW_RUNTIME_ERROR("No ca certs file was found for GRPC.");
        }
        ssl_options.pem_root_certs = read_file(certsfn);
        return ssl_options;
    }();

    auto creds = grpc::SslCredentials(ssl_options);
    auto channel = grpc::CreateChannel(api_url, secure ? creds : grpc::InsecureChannelCredentials());
    return channel;
}

String Remote::DatabaseInformation::getVersionUrl() const
{
    return version_root_url + "/" + sw::getPackagesDatabaseVersionFileName();
}

int Remote::DatabaseInformation::getVersion() const
{
    static std::mutex m;
    std::unique_lock lk(m);

    if (version != -1)
        return version;

    version = [this]()
    {
        LOG_TRACE(logger, "Checking remote version");
        try
        {
            auto ver = local_dir.empty() ? download_file(getVersionUrl()) : read_file(getVersionUrl());
            return std::stoi(ver);
        }
        catch (std::exception &e)
        {
            LOG_DEBUG(logger, "Couldn't download db version file: " << e.what());
        }
        return 0;
    }();

    return version;
}

}
