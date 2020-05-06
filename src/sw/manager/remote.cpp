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

#include <boost/dll.hpp>
#include <fmt/ostream.h>
#include <primitives/http.h>
#include <primitives/templates.h>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "remote");

namespace sw
{

Remotes get_default_remotes()
{
    static Remotes rms;
    RUN_ONCE
    {
        Remote r;
        r.name = DEFAULT_REMOTE_NAME;
        r.url = "https://software-network.org/";
        rms.push_back(r);
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
        auto sfh = get_strong_file_hash(fn, d.getData().hash);
        if (sfh == d.getData().hash)
        {
            dl_hash = sfh;
            return true;
        }
        auto fh = get_file_hash(fn);
        if (fh == d.getData().hash)
        {
            dl_hash = fh;
            return true;
        }
        LOG_TRACE(logger, "Downloaded file: " << url << " hash = " << sfh);
    }
    return false;
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

    auto p = url.find("://");
    auto host = url.substr(p == url.npos ? 0 : p + 3);
    host = host.substr(0, host.find('/'));
    if (host.find(':') == host.npos)
    {
        //host = host.substr(0, host.find(':')); // remove port
        if (host.find("api") != 0)
            host = "api." + host;
    }

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
    auto channel = grpc::CreateChannel(host, secure ? creds : grpc::InsecureChannelCredentials());
    return channel;
}

}
