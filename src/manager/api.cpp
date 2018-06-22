// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "api.h"

#include "http.h"
#include "package_path.h"
#include "property_tree.h"
#include "settings.h"

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "api");

namespace sw
{

ptree api_call(const Remote &r, const String &api, ptree request)
{
    if (r.user.empty())
        throw std::runtime_error("Remote user is empty");
    if (r.token.empty())
        throw std::runtime_error("Remote token is empty");

    request.put("auth.user", r.user);
    request.put("auth.token", r.token);

    HttpRequest req = httpSettings;
    req.type = HttpRequest::Post;
    req.url = r.url + "/api/" + api;
    req.data = ptree2string(request);
    auto resp = url_request(req);
    auto ret = string2ptree(resp.response);
    if (resp.http_code != 200)
    {
        auto e = ret.get<String>("error", "");
        throw std::runtime_error(e);
    }

    return string2ptree(resp.response);
}

void check_relative(const Remote &r, PackagePath &p)
{
    if (p.isRelative(r.user))
        p = "pvt." + r.user + "." + p.toString();
}

void Api::add_project(const Remote &r, PackagePath p)
{
    check_relative(r, p);
    ptree request;
    request.put("project", p.toString());
    api_call(r, "add_project", request);
}

void Api::remove_project(const Remote &r, PackagePath p)
{
    check_relative(r, p);
    ptree request;
    request.put("project", p.toString());
    api_call(r, "remove_project", request);
}

void Api::add_version(const Remote &r, PackagePath p, const String &cppan)
{
    check_relative(r, p);
    ptree request;
    request.put("project", p.toString());
    request.put("cppan", cppan);
    api_call(r, "add_version", request);
}

void Api::add_version(const Remote &r, PackagePath p, const Version &vnew)
{
    add_version(r, p, vnew, String());
}

void Api::add_version(const Remote &r, PackagePath p, const Version &vnew, const String &vold)
{
    check_relative(r, p);
    ptree request;
    request.put("project", p.toString());
    request.put("new", vnew.toString());
    if (!vold.empty())
        request.put("old", vold);
    api_call(r, "add_version", request);
}

void Api::update_version(const Remote &r, PackagePath p, const Version &v)
{
    if (!v.isBranch())
        throw std::runtime_error("Only branches can be updated");
    check_relative(r, p);
    ptree request;
    request.put("project", p.toString());
    request.put("version", v.toString());
    api_call(r, "update_version", request);
}

void Api::remove_version(const Remote &r, PackagePath p, const Version &v)
{
    check_relative(r, p);
    ptree request;
    request.put("project", p.toString());
    request.put("version", v.toString());
    api_call(r, "remove_version", request);
}

void Api::get_notifications(const Remote &r, int n)
{
    if (n < 0)
        return;

    ptree request;
    request.put("n", n);
    auto response = api_call(r, "get_notifications", request);
    auto notifications = response.get_child("notifications");
    int i = 1;
    for (auto &n : notifications)
    {
        auto nt = (NotificationType)n.second.get<int>("type", 0);
        auto t = n.second.get<String>("text", "");
        auto ts = n.second.get<String>("timestamp", "");

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
        LOG_INFO(logger, ss.str() << " " << ts << " " << t);
    }
}

void Api::clear_notifications(const Remote &r)
{
    ptree request;
    api_call(r, "clear_notifications", request);
}

}
