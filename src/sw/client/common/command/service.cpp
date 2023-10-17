// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2023 Egor Pugin <egor.pugin@gmail.com>

#include "../commands.h"

#include <sw/manager/package_database.h>
#include <sw/manager/storage_remote.h>
#include <sw/support/source.h>
#include <nlohmann/json.hpp>
#include <primitives/http.h>

#include <format>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "service");

#define F_ARGS SwClientContext &swctx, sw::LocalStorage &sdb, const sw::LocalPackage &p
#ifdef _MSC_VER
#define F(n, ...) static void n(F_ARGS, __VA_ARGS__)
#else
#define F(n, ...) static void n(F_ARGS, ##__VA_ARGS__)
#endif

void update_packages(SwClientContext &swctx) {
    struct data {
        long http_code = 0;
        String response;
        //       new version                old version
        std::map<sw::Version, std::multimap<sw::Version, sw::PackageId>> packages;
    };
    std::map<std::string, data> new_versions;
    auto &s = *swctx.getContext().getRemoteStorages().at(0);
    auto &rs = dynamic_cast<sw::RemoteStorage&>(s);
    auto &pdb = rs.getPackagesDatabase();
    for (auto &&ppath : pdb.getMatchingPackages("org.sw.demo.")) {
        auto versions = pdb.getVersionsForPackage(ppath);
        if (versions.empty() || versions.rbegin()->isBranch()) {
            continue;
        }
        auto &maxver = *versions.rbegin();
        sw::UnresolvedPackages pkgs;
        pkgs.insert({ppath,maxver});
        sw::UnresolvedPackages upkgs;
        auto &&resolved = pdb.resolve(pkgs, upkgs);
        auto &&d = pdb.getPackageData(resolved.begin()->second);
        if (d.source.empty()) {
            LOG_INFO(logger, "empty source: " << resolved.begin()->second.toString());
            continue;
        }
        auto s = sw::source::load(nlohmann::json::parse(d.source));
        if (s->getType() != primitives::source::SourceType::Git) {
            continue;
        }
        auto git = dynamic_cast<primitives::source::Git*>(s.get());
        if (git->tag.empty()) {
            continue;
        }
        if (new_versions[d.source].http_code == 0) {
            HttpRequest request{httpSettings};
            request.url = git->url + "/info/refs?service=git-upload-pack";
            auto resp = url_request(request);
            new_versions[d.source].http_code = resp.http_code;
            new_versions[d.source].response = resp.response;
        }
        if (new_versions[d.source].http_code != 200) {
            LOG_INFO(logger, "http " << new_versions[d.source].http_code << ": " << resolved.begin()->second.toString());
            continue;
        }
        for (auto &&line : split_lines(new_versions[d.source].response)) {
            if (!line.contains("refs/tags/") || line.contains("^")) {
                continue;
            }
            auto ver = line.substr(line.rfind('/') + 1);
            constexpr auto digits = "0123456789";
            std::vector<std::string> numbers;
            size_t p = 0;
            while (1) {
                p = ver.find_first_of(digits, p);
                if (p == -1) {
                    break;
                }
                auto end = ver.find_first_not_of(digits, p);
                if (end == -1) {
                    numbers.push_back(ver.substr(p));
                    break;
                }
                numbers.push_back(ver.substr(p, end - p));
                ver = ver.substr(end);
            }
            if (numbers.empty()) {
                continue;
            }
            ver.clear();
            for (auto &&n : numbers) {
                ver += n + ".";
            }
            ver.pop_back();
            try {
                sw::Version v{ver};
                if (v > maxver && v.isRelease()) {
                    auto source = d.source;
                    boost::replace_all(source, maxver.toString(), "{v}");
                    auto s = sw::source::load(nlohmann::json::parse(source));
                    auto git = dynamic_cast<primitives::source::Git *>(s.get());
                    auto apply = [&](auto g) {
                        g->applyVersion(v);
                        if (line.ends_with("refs/tags/" + g->tag)) {
                            new_versions[d.source].packages[v].insert({maxver, resolved.begin()->second});
                            LOG_INFO(logger, "new version: " << resolved.begin()->second.toString() << ": " << v.toString());
                            return true;
                        }
                        return false;
                    };
                    if (!apply(git)) {
                        // try other tag check {v} -> {M}.{m}{po}
                        auto source = d.source;
                        boost::replace_all(source, maxver.toString(), "{v}");
                        auto s = sw::source::load(nlohmann::json::parse(source));
                        auto git = dynamic_cast<primitives::source::Git *>(s.get());
                        boost::replace_all(git->tag, "{v}", "{M}.{m}{po}");
                        if (apply(git)) {
                            LOG_INFO(logger, "tag fixed: " << resolved.begin()->second.toString() << ": " << v.toString());
                        } else {
                            LOG_INFO(logger, "tag check error: " << resolved.begin()->second.toString() << ": " << v.toString());
                        }
                    }
                }
            } catch (std::runtime_error &e) {
                LOG_INFO(logger, "bad version: " << ver << "(line: '" << line << "'): " << e.what());
            }
        }
    }
    LOG_INFO(logger, "\ncommand list\n");
    std::map<sw::PackageId, std::pair<sw::Version, int>> new_pkgs;
    for (auto &&[_,n] : new_versions) {
        if (n.packages.empty()) {
            continue;
        }
        auto &&p = n.packages.rbegin();
        auto &&v = p->first;
        auto &&pkg = n.packages.rbegin()->second.rbegin()->second;
        auto &&d = pdb.getPackageData(pkg);
        new_pkgs.emplace(pkg, std::pair<sw::Version, int>{v, d.prefix});
    }
    for (auto &&[p, vp] : new_pkgs) {
        auto &&v = vp.first;
        auto &&prefix = vp.second;
        LOG_INFO(logger, std::format("sw uri --silent sw:upload {} {} {}", p.toString(), v.toString(), prefix));
    }
}

SUBCOMMAND_DECL(service)
{
#define CMD(f, ...)                                 \
    if (getOptions().options_service.command == #f) \
    {                                               \
        f(*this);                                   \
        return;                                     \
    }
    CMD(update_packages);
}
