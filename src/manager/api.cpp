// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "api.h"

#include "package_path.h"
#include "remote.h"
#include "settings.h"

#include <grpcpp/grpcpp.h>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "api");

namespace sw
{

void check_relative(const Remote &r, PackagePath &p)
{
    if (p.isRelative(r.user))
        p = "pvt." + r.user + "." + p.toString();
}

void apply_auth(const Remote &r, grpc::ClientContext &context)
{
    context.AddMetadata("auth.user", r.user);
    context.AddMetadata("auth.token", r.token);
}

void check_status(grpc::Status status)
{
    if (!status.ok())
        LOG_ERROR(logger, "RPC failed: " << status.error_code() << ": " << status.error_message());
}

void check_status_and_throw(grpc::Status status)
{
    if (!status.ok())
        throw std::runtime_error("RPC failed: " + std::to_string(status.error_code()) + ": " + status.error_message());
}

Api::Api(const Remote &r)
    : r(r)
    , api_(api::ApiService::NewStub(r.getGrpcChannel()))
    , user_(api::UserService::NewStub(r.getGrpcChannel()))
{
}

void Api::addDownloads(const std::set<int64_t> &pkgs)
{
    api::PackageIds request;
    for (auto &id : pkgs)
        request.mutable_ids()->Add(id);
    grpc::ClientContext context;
    check_status(api_->AddDownloads(&context, request, nullptr));
}

void Api::addClientCall()
{
    google::protobuf::Empty request;
    grpc::ClientContext context;
    check_status(api_->AddClientCall(&context, request, nullptr));
}

IdDependencies Api::resolvePackages(const UnresolvedPackages &pkgs)
{
    api::UnresolvedPackages request;
    for (auto &pkg : pkgs)
    {
        auto pb_pkg = request.mutable_packages()->Add();
        pb_pkg->set_path(pkg.ppath);
        pb_pkg->set_range(pkg.range.toString());
    }
    api::ResolvedPackages resolved;
    grpc::ClientContext context;
    check_status_and_throw(api_->ResolvePackages(&context, request, &resolved));

    IdDependencies id_deps;
    for (auto &pkg : resolved.packages())
    {
        DownloadDependency d;
        d.ppath = pkg.package().path();
        d.version = pkg.package().version();
        d.flags = pkg.flags();
        d.hash = pkg.hash();
        d.group_number = pkg.group_number();

        std::unordered_set<db::PackageVersionId> idx;
        for (auto &tree_dep : pkg.dependencies())
            idx.insert(tree_dep);
        d.setDependencyIds(idx);
    }
    return id_deps;
}

void Api::addVersion(const String &cppan)
{
    api::NewPackage request;
    request.set_script(cppan);

    grpc::ClientContext context;
    apply_auth(r, context);

    check_status(user_->AddPackage(&context, request, nullptr));
}

void Api::addVersion(PackagePath p, const Version &vnew, const optional<Version> &vold)
{
    check_relative(r, p);

    api::NewPackage request;
    request.mutable_version()->mutable_package()->set_path(p.toString());
    request.mutable_version()->mutable_package()->set_version(vnew.toString());
    if (vold)
        request.mutable_version()->set_old_version(vold.value().toString());

    grpc::ClientContext context;
    apply_auth(r, context);

    check_status(user_->AddPackage(&context, request, nullptr));
}

void Api::updateVersion(PackagePath p, const Version &v)
{
    if (!v.isBranch())
        throw std::runtime_error("Only branches can be updated");

    check_relative(r, p);

    api::PackageId request;
    request.set_path(p.toString());
    request.set_version(v.toString());

    grpc::ClientContext context;
    apply_auth(r, context);

    check_status(user_->UpdatePackage(&context, request, nullptr));
}

void Api::removeVersion(PackagePath p, const Version &v)
{
    check_relative(r, p);

    api::PackageId request;
    request.set_path(p.toString());
    request.set_version(v.toString());

    grpc::ClientContext context;
    apply_auth(r, context);

    check_status(user_->UpdatePackage(&context, request, nullptr));
}

void Api::getNotifications(int n)
{
    if (n < 0)
        return;

    api::NotificationsRequest request;
    request.set_n(n);

    api::Notifications notifications;

    grpc::ClientContext context;
    apply_auth(r, context);

    check_status_and_throw(user_->GetNotifications(&context, request, &notifications));

    int i = 1;
    for (auto &n : notifications.notifications())
    {
        auto nt = (NotificationType)n.type();
        std::ostringstream ss;
        ss << i++ << " ";
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
    grpc::ClientContext context;
    check_status(user_->ClearNotification(&context, request, nullptr));
}

}
