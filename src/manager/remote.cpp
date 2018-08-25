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

//#include "logger.h"
//DECLARE_STATIC_LOGGER(logger, "remote");

namespace sw
{

Remotes get_default_remotes()
{
    static Remotes rms;
    RUN_ONCE
    {
        Remote r;
        r.name = DEFAULT_REMOTE_NAME;
#ifdef _WIN32
        r.url = "http://localhost:55555/";
#else
        r.url = "http://192.168.191.1:55555/";
#endif
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
    if (!channel)
    {
        auto p = url.find("://");
        auto host = url.substr(p == url.npos ? 0 : p + 3);
        host = host.substr(0, host.find('/'));
        host = host.substr(0, host.find(':'));

        grpc::SslCredentialsOptions ssl_options;
#ifdef _WIN32
        ssl_options.pem_root_certs = read_file("d:\\dev\\cppan2\\bin\\server.crt");
#else
        ssl_options.pem_root_certs = read_file("/home/egor/dev/sw_server.crt");
        host = "192.168.191.1";
#endif

        auto creds = grpc::SslCredentials(ssl_options);
        channel = grpc::CreateChannel(host, creds);
    }
    return channel;
}

}
