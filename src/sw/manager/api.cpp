// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "api.h"

#include "package_path.h"
#include "remote.h"
#include "settings.h"

#include <sw/protocol/grpc_helpers.h>
#include <sw/support/exceptions.h>

#include <grpcpp/grpcpp.h>
#include <nlohmann/json.hpp>
#include <primitives/templates.h>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "api");

namespace sw
{

void check_relative(const Remote &r, PackagePath &p)
{
    throw SW_RUNTIME_ERROR("not implemented");

    //if (p.isRelative(r.user))
        //p = "pvt." + r.user + "." + p.toString();
}

void apply_auth(const Remote &r, grpc::ClientContext &context)
{
    if (r.publishers.empty())
        throw SW_RUNTIME_ERROR("Empty publishers");
    context.AddMetadata(SW_GRPC_METADATA_AUTH_USER, r.publishers.begin()->second.name);
    context.AddMetadata(SW_GRPC_METADATA_AUTH_TOKEN, r.publishers.begin()->second.token);
}

Api::Api(const Remote &r)
    : r(r)
    , api_(api::ApiService::NewStub(r.getGrpcChannel()))
    , user_(api::UserService::NewStub(r.getGrpcChannel()))
{
}

std::unique_ptr<grpc::ClientContext> Api::getContext()
{
    auto context = std::make_unique<grpc::ClientContext>();
    GRPC_SET_DEADLINE(10);
    context->AddMetadata(SW_GRPC_METADATA_CLIENT_VERSION, "0.3.0");
    return context;
}

std::unique_ptr<grpc::ClientContext> Api::getContextWithAuth()
{
    auto ctx = getContext();
    apply_auth(r, *ctx);
    return ctx;
}

void Api::addDownloads(const std::set<int64_t> &pkgs)
{
    api::PackageIds request;
    for (auto &id : pkgs)
        request.mutable_ids()->Add(id);
    auto context = getContext();
    GRPC_CALL(api_, AddDownloads, google::protobuf::Empty);
}

void Api::addClientCall()
{
    google::protobuf::Empty request;
    auto context = getContext();
    GRPC_CALL(api_, AddClientCall, google::protobuf::Empty);
}

Api::IdDependencies Api::resolvePackages(const UnresolvedPackages &pkgs)
{
    api::UnresolvedPackages request;
    for (auto &pkg : pkgs)
    {
        auto pb_pkg = request.mutable_packages()->Add();
        pb_pkg->set_path(pkg.ppath);
        pb_pkg->set_range(pkg.range.toString());
    }
    auto context = getContext();
    GRPC_CALL_THROWS(api_, ResolvePackages, api::ResolvedPackages);

    IdDependencies id_deps;
    for (auto &pkg : response.packages())
    {
        RemotePackageData d(pkg.package().path(), pkg.package().version());
        d.id = pkg.id();
        d.flags = pkg.flags();
        d.hash = pkg.hash();
        d.group_number = pkg.group_number();
        d.prefix = pkg.prefix();

        for (auto &tree_dep : pkg.dependencies())
            d.deps.insert(tree_dep.id());
        id_deps.emplace(d.id, d);
    }
    return id_deps;
}

void Api::addVersion(PackagePath prefix, const PackageDescriptionMap &pkgs, const String &script)
{
    api::NewPackage request;
    request.mutable_package_data()->mutable_script()->set_script(script);
    request.mutable_package_data()->mutable_script()->set_prefix_path(prefix.toString());
    nlohmann::json jm;
    for (auto &[pkg, d] : pkgs)
    {
        auto j = nlohmann::json::parse(d->getString());
        auto rd = j["root_dir"].get<String>();
        auto sz = rd.size();
        if (rd.back() != '\\' && rd.back() != '/')
            sz++;
        for (auto &f : j["files"])
            f["from"] = f["from"].get<String>().substr(sz);
        jm["packages"][pkg.toString()] = j;
    }
    request.mutable_package_data()->set_data(jm.dump());
    auto context = getContextWithAuth();
    GRPC_SET_DEADLINE(10);
    GRPC_CALL_THROWS(user_, AddPackage, google::protobuf::Empty);
}

void Api::addVersion(const PackagePath &prefix, const String &script)
{
    api::NewPackage request;
    request.mutable_script()->set_script(script);
    request.mutable_script()->set_prefix_path(prefix.toString());
    auto context = getContextWithAuth();
    GRPC_SET_DEADLINE(10);
    GRPC_CALL_THROWS(user_, AddPackage, google::protobuf::Empty);
}

void Api::addVersion(PackagePath p, const Version &vnew, const std::optional<Version> &vold)
{
    check_relative(r, p);

    api::NewPackage request;
    request.mutable_version()->mutable_package()->set_path(p.toString());
    request.mutable_version()->mutable_package()->set_version(vnew.toString());
    if (vold)
        request.mutable_version()->set_old_version(vold.value().toString());

    auto context = getContextWithAuth();
    GRPC_SET_DEADLINE(300);
    GRPC_CALL_THROWS(user_, AddPackage, google::protobuf::Empty);
}

void Api::updateVersion(PackagePath p, const Version &v)
{
    if (!v.isBranch())
        throw SW_RUNTIME_ERROR("Only branches can be updated");

    check_relative(r, p);

    api::PackageId request;
    request.set_path(p.toString());
    request.set_version(v.toString());

    auto context = getContextWithAuth();
    GRPC_SET_DEADLINE(300);
    GRPC_CALL_THROWS(user_, UpdatePackage, google::protobuf::Empty);
}

void Api::removeVersion(PackagePath p, const Version &v)
{
    check_relative(r, p);

    api::PackageId request;
    request.set_path(p.toString());
    request.set_version(v.toString());

    auto context = getContextWithAuth();
    GRPC_CALL_THROWS(user_, RemovePackage, google::protobuf::Empty);
}

void Api::getNotifications(int n)
{
    if (n < 0)
        return;

    api::NotificationsRequest request;
    request.set_n(n);

    auto context = getContextWithAuth();
    GRPC_CALL_THROWS(user_, GetNotifications, api::Notifications);

    // move out; return as result
    for (const auto &[i,n] : enumerate(response.notifications()))
    {
        auto nt = (NotificationType)n.type();
        std::ostringstream ss;
        ss << i + 1 << " ";
        switch (nt)
        {
        case NotificationType::Error:
            ss << "E";
            break;
        case NotificationType::Warning:
            ss << "W";
            break;
        case NotificationType::Message:
            ss << "I";
            break;
        case NotificationType::Success:
            ss << "OK";
            break;
        }
        LOG_INFO(logger, ss.str() << " " << n.timestamp() << " " << n.text());
    }
}

void Api::clearNotifications()
{
    google::protobuf::Empty request;
    auto context = getContextWithAuth();
    GRPC_CALL_THROWS(user_, ClearNotification, google::protobuf::Empty);
}

}
