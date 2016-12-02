/*
 * Copyright (c) 2016, Egor Pugin
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     1. Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *     2. Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the distribution.
 *     3. Neither the name of the copyright holder nor the names of
 *        its contributors may be used to endorse or promote products
 *        derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "api.h"

#include "http.h"
#include "log.h"
#include "project.h"
#include "settings.h"

#include "logger.h"
DECLARE_STATIC_LOGGER(logger, "api");

ptree api_call(const Remote &r, const String &api, ptree request)
{
    request.put("auth.user", r.user);
    request.put("auth.token", r.token);

    HttpRequest req = httpSettings;
    req.type = HttpRequest::POST;
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

void check_relative(const Remote &r, ProjectPath &p)
{
    if (p.is_relative(r.user))
        p = "pvt." + r.user + "." + p.toString();
}

void Api::add_project(const Remote &r, ProjectPath p, ProjectType t)
{
    check_relative(r, p);
    ptree request;
    request.put("project", p.toString());
    request.put("type", toIndex(t));
    api_call(r, "add_project", request);
}

void Api::remove_project(const Remote &r, ProjectPath p)
{
    check_relative(r, p);
    ptree request;
    request.put("project", p.toString());
    api_call(r, "remove_project", request);
}

void Api::add_version(const Remote &r, ProjectPath p, const String &cppan)
{
    check_relative(r, p);
    ptree request;
    request.put("project", p.toString());
    request.put("cppan", cppan);
    api_call(r, "add_version", request);
}

void Api::remove_version(const Remote &r, ProjectPath p, const Version &v)
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
        std::cout << i++ << " ";
        switch (nt)
        {
            case NotificationType::Error:
                std::cout << "E";
                break;
            case NotificationType::Warning:
                std::cout << "W";
                break;
            case NotificationType::Message:
                std::cout << "I";
                break;
            case NotificationType::Success:
                std::cout << "OK";
                break;
        }
        std::cout << " " << ts << " " << t << "\n";
    }
}

void Api::clear_notifications(const Remote &r)
{
    ptree request;
    api_call(r, "clear_notifications", request);
}
