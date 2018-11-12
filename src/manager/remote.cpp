// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "remote.h"

#include "directories.h"
#include "package.h"

#include <hash.h>
#include <http.h>

#include <fmt/ostream.h>
#include <primitives/templates.h>
#include <grpcpp/grpcpp.h>
#include <boost/dll.hpp>

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
/*#ifdef _WIN32
        r.url = "http://localhost:55555/";
#else
        r.url = "http://192.168.191.1:55555/";
#endif*/
        r.url = "https://software-network.org/";
        rms.push_back(r);
    };
    return rms;
}

String DataSource::getUrl(const PackageId &pkg) const
{
    // {DD} = base data dir
    // {PHPF} = package hash path full
    // {PH64} = package hash, length = 64
    // {FN} = archive name
    return fmt::format(raw_url,
        fmt::arg("DD", pkg.isPrivate() ? getDataDirPrivate() : getDataDir()),
        fmt::arg("PHPF", normalize_path(pkg.getHashPathFull())),
        fmt::arg("PH64", pkg.getHash().substr(0, 64)),
        fmt::arg("FN", make_archive_name())
    );
}

bool DataSource::downloadPackage(const Package &d, const String &hash, const path &fn, bool try_only_first) const
{
    auto download_from_source = [&](const auto &url)
    {
        try
        {
            LOG_TRACE(logger, "Downloading file: " << url);
            download_file(url, fn);
        }
        catch (const std::exception&)
        {
            return false;
        }
        return check_strong_file_hash(fn, hash) || check_file_hash(fn, hash);
    };

    if (download_from_source(getUrl(d)))
        return true;
    return false;
}

std::shared_ptr<grpc::Channel> Remote::getGrpcChannel() const
{
    if (channel)
        return channel;

    static std::mutex m;
    std::unique_lock lk(m);

    auto p = url.find("://");
    auto host = url.substr(p == url.npos ? 0 : p + 3);
    host = host.substr(0, host.find('/'));
    host = host.substr(0, host.find(':'));
    host = "api." + host;

    //auto host = url;
    //host += "api/";

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

    auto creds = grpc::SslCredentials(ssl_options);

/*#ifdef _WIN32
    auto crt = path(boost::dll::program_location().parent_path().string()) / "server.crt";
    if (fs::exists(crt))
        ssl_options.pem_root_certs = read_file(crt);
    else
        ssl_options.pem_root_certs = read_file("d:\\dev\\cppan2\\bin\\server.crt");
    auto creds = grpc::SslCredentials(ssl_options);
#else
    ssl_options.pem_root_certs = read_file("/home/egor/dev/sw_server.crt");
    host = "192.168.191.1:1245";
    auto creds = grpc::InsecureChannelCredentials();
#endif*/

    channel = grpc::CreateChannel(host, creds);
    return channel;
}

}
