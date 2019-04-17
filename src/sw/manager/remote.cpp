// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "remote.h"

#include "database.h"
#include "package.h"
#include "storage.h"

#include <sw/support/hash.h>

#include <boost/dll.hpp>
#include <fmt/ostream.h>
#include <grpcpp/grpcpp.h>
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

std::shared_ptr<grpc::Channel> Remote::getGrpcChannel() const
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
        host = "api." + host;
    }

    static const grpc::SslCredentialsOptions ssl_options = []()
    {
        static const path cert_dir = get_root_directory() / "certs";
        path cert_file = cert_dir / "roots.pem";

        grpc::SslCredentialsOptions ssl_options;
#ifdef _WIN32
        if (!fs::exists(cert_file))
            download_file("https://raw.githubusercontent.com/grpc/grpc/master/etc/roots.pem", cert_file);
        ssl_options.pem_root_certs = read_file(cert_file);
#else
        cert_file = "/etc/ssl/certs/ca-certificates.crt";
        if (!fs::exists(cert_file))
            cert_file = "/etc/ssl/certs/ca-bundle.crt";
        if (!fs::exists(cert_file))
        {
            cert_file = cert_dir / "roots.pem";
            download_file("https://raw.githubusercontent.com/grpc/grpc/master/etc/roots.pem", cert_file);
        }
        ssl_options.pem_root_certs = read_file(cert_file);
#endif
        return ssl_options;
    }();

    auto creds = grpc::SslCredentials(ssl_options);
    auto channel = grpc::CreateChannel(host, secure ? creds : grpc::InsecureChannelCredentials());
    return channel;
}

}
