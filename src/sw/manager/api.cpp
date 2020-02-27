// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "api.h"
#include "api_protobuf.h"

#include "package_path.h"
#include "remote.h"
#include "settings.h"

#include <sw/protocol/grpc_helpers.h>
#include <sw/support/exceptions.h>

#include <nlohmann/json.hpp>
#include <primitives/templates.h>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "api");

namespace sw
{

static void check_relative(const Remote &r, PackagePath &p)
{
    throw SW_RUNTIME_ERROR("not implemented");

    //if (p.isRelative(r.user))
        //p = "pvt." + r.user + "." + p.toString();
}

static void apply_auth(const Remote &r, grpc::ClientContext &context)
{
    if (r.publishers.empty())
        throw SW_RUNTIME_ERROR("Empty publishers");
    context.AddMetadata(SW_GRPC_METADATA_AUTH_USER, r.publishers.begin()->second.name);
    context.AddMetadata(SW_GRPC_METADATA_AUTH_TOKEN, r.publishers.begin()->second.token);
}

Api::~Api()
{
}

ProtobufApi::ProtobufApi(const Remote &r)
    : r(r)
    , c(r.getGrpcChannel())
    , api_(api::ApiService::NewStub(c))
    , user_(api::UserService::NewStub(c))
{
}

std::unique_ptr<grpc::ClientContext> ProtobufApi::getContext() const
{
    auto context = std::make_unique<grpc::ClientContext>();
    GRPC_SET_DEADLINE(10);
    return context;
}

std::unique_ptr<grpc::ClientContext> ProtobufApi::getContextWithAuth() const
{
    auto ctx = getContext();
    apply_auth(r, *ctx);
    return ctx;
}

std::unordered_map<UnresolvedPackage, PackagePtr> ProtobufApi::resolvePackages(const UnresolvedPackages &pkgs, UnresolvedPackages &unresolved_pkgs,
    std::unordered_map<PackageId, PackageData> &data, const IStorage &s) const
{
    api::UnresolvedPackages request;
    for (auto &pkg : pkgs)
    {
        auto pb_pkg = request.mutable_unresolved_packages()->Add();
        pb_pkg->set_path(pkg.ppath);
        pb_pkg->set_range(pkg.range.toString());
    }
    auto context = getContext();
    GRPC_CALL_THROWS(api_, ResolvePackages, api::ResolvedPackages);

    // process result

    // read unresolved
    for (auto &u : response.unresolved_packages().unresolved_packages())
        unresolved_pkgs.emplace(u.path(), u.range());

    // read resolved
    std::unordered_map<UnresolvedPackage, PackagePtr> m;
    for (auto &pair : response.resolved_packages())
    {
        auto &pkg = pair.resolved_package();

        PackageId p(pkg.package().path(), pkg.package().version());

        PackageData d;
        d.flags = pkg.flags();
        d.hash = pkg.hash();
        d.prefix = pkg.prefix();
        for (auto &tree_dep : pkg.dependencies().unresolved_packages())
            d.dependencies.emplace(tree_dep.path(), tree_dep.range());
        data[p] = d;

        m[{pair.unresolved_package().path(), pair.unresolved_package().range()}] = std::make_unique<Package>(s, p);
    }
    return m;
}

void ProtobufApi::addVersion(PackagePath prefix, const PackageDescriptionMap &pkgs, const String &script_name, const String &script) const
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
        {
            auto s = f["from"].get<String>();
            if (s.find(rd) != 0)
                throw SW_RUNTIME_ERROR("bad file path: " + s);
            f["from"] = s.substr(sz);
        }
        jm["packages"][pkg.toString()] = j;
    }
    jm["script_name"] = script_name;
    request.mutable_package_data()->set_data(jm.dump());
    auto context = getContextWithAuth();
    GRPC_SET_DEADLINE(10);
    GRPC_CALL_THROWS(user_, AddPackage, google::protobuf::Empty);
}

void ProtobufApi::addVersion(const PackagePath &prefix, const String &script)
{
    api::NewPackage request;
    request.mutable_script()->set_script(script);
    request.mutable_script()->set_prefix_path(prefix.toString());
    auto context = getContextWithAuth();
    GRPC_SET_DEADLINE(10);
    GRPC_CALL_THROWS(user_, AddPackage, google::protobuf::Empty);
}

void ProtobufApi::addVersion(PackagePath p, const Version &vnew, const std::optional<Version> &vold)
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

void ProtobufApi::updateVersion(PackagePath p, const Version &v)
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

void ProtobufApi::removeVersion(PackagePath p, const Version &v)
{
    check_relative(r, p);

    api::PackageId request;
    request.set_path(p.toString());
    request.set_version(v.toString());

    auto context = getContextWithAuth();
    GRPC_CALL_THROWS(user_, RemovePackage, google::protobuf::Empty);
}

void ProtobufApi::getNotifications(int n)
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

void ProtobufApi::clearNotifications()
{
    google::protobuf::Empty request;
    auto context = getContextWithAuth();
    GRPC_CALL_THROWS(user_, ClearNotification, google::protobuf::Empty);
}

}
