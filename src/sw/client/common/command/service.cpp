// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2023 Egor Pugin <egor.pugin@gmail.com>

#include "../commands.h"

#include <sw/manager/package_database.h>
#include <sw/manager/storage_remote.h>
#include <sw/support/source.h>
#include <nlohmann/json.hpp>
#include <primitives/http.h>

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
    String prefix = "org.sw.demo.";
    if (!swctx.getOptions().options_service.args.empty()) {
        prefix = swctx.getOptions().options_service.args[0];
    }
    auto all_pkgs = pdb.getMatchingPackages(prefix);
    for (int pkgid = 0; auto &&ppath : all_pkgs) {
        LOG_INFO(logger, "[" << ++pkgid << "/" << all_pkgs.size() << "] " << ppath.toString());
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
            LOG_DEBUG(logger, "empty source: " << resolved.begin()->second.toString());
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
        auto &source_id = git->url; // d.source has real tag so it is now useful
        if (new_versions[source_id].http_code == 0) {
            HttpRequest request{httpSettings};
            request.url = git->url + "/info/refs?service=git-upload-pack";
            try {
                auto resp = url_request(request);
                new_versions[source_id].http_code = resp.http_code;
                new_versions[source_id].response = resp.response;
            } catch (std::exception &) {
            }
        }
        if (new_versions[source_id].http_code != 200) {
            LOG_WARN(logger, "http " << new_versions[source_id].http_code << ": " << resolved.begin()->second.toString());
            continue;
        }
        for (auto &&line : split_lines(new_versions[source_id].response)) {
            if (!line.contains("refs/tags/") || line.contains("^")) {
                continue;
            }
            auto ver = line.substr(line.rfind('/') + 1);
            constexpr auto digits = "0123456789";
            std::vector<std::string> numbers;
            while (1) {
                size_t p = 0;
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
                // skip pre releases
                if (std::ranges::find_if(ver, isalpha) != ver.end()) {
                    numbers.clear();
                    break;
                }
            }
            if (numbers.empty()) {
                continue;
            }
            ver.clear();
            for (int i = 0; auto &&n : numbers) {
                if (i == 0 && n.size() == 8) {
                    // YYYYMMDD
                    ver += n.substr(0, 4) + ".";
                    ver += n.substr(4, 2) + ".";
                    ver += n.substr(6, 2) + ".";
                } else {
                    ver += n + ".";
                }
                ++i;
            }
            ver.pop_back();
            try {
                sw::Version v{ver};
                if (v > maxver && v.isRelease()) {
                    bool ok = false;
                    {
                        auto source = d.source;
                        auto s = sw::source::load(nlohmann::json::parse(source));
                        auto git = dynamic_cast<primitives::source::Git *>(s.get());
                        int pos = 0;
                        for (int i = 0; i < v.getLevel(); ++i) {
                            auto tofind = std::to_string(maxver[i]);
                            pos = git->tag.find(tofind, pos);
                            if (pos == -1) {
                                LOG_WARN(logger, std::format("cant find {} in {}", tofind, git->tag));
                                break;
                            }
                            git->tag = git->tag.substr(0, pos) + std::to_string(v[i]) + git->tag.substr(pos + tofind.size());
                            pos += tofind.size();
                            //LOG_INFO(logger, git->tag);
                        }
                        if (line.ends_with("refs/tags/" + git->tag)) {
                            new_versions[source_id].packages[v].insert({maxver, resolved.begin()->second});
                            LOG_INFO(logger, "new version: " << resolved.begin()->second.toString() << ": " << v.toString());
                            ok = true;
                        }
                    }

                    if (0 && !ok) {
                        auto source = d.source;
                        boost::replace_all(source, maxver.toString(), "{v}");
                        auto s = sw::source::load(nlohmann::json::parse(source));
                        auto git = dynamic_cast<primitives::source::Git *>(s.get());
                        auto apply = [&](auto g) {
                            g->applyVersion(v);
                            if (line.ends_with("refs/tags/" + g->tag)) {
                                new_versions[source_id].packages[v].insert({maxver, resolved.begin()->second});
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
                                if (maxver.getPatch() == 0) {
                                    auto verstring = maxver.toString();
                                    verstring.resize(verstring.size() - 2); // remove .0
                                    auto source = d.source;
                                    boost::replace_all(source, verstring, "{M}.{m}{po}");
                                    auto s = sw::source::load(nlohmann::json::parse(source));
                                    auto git = dynamic_cast<primitives::source::Git *>(s.get());
                                    if (apply(git)) {
                                        LOG_INFO(logger, "tag fixed: " << resolved.begin()->second.toString() << ": " << v.toString());
                                    } else {
                                        LOG_DEBUG(logger, "tag check error: " << resolved.begin()->second.toString() << ": " << v.toString());
                                    }
                                } else {
                                    LOG_DEBUG(logger, "tag check error: " << resolved.begin()->second.toString() << ": " << v.toString());
                                }
                            }
                        }
                    }
                }
            } catch (std::runtime_error &e) {
                LOG_WARN(logger, "bad version: " << ver << "(line: '" << line << "'): " << e.what());
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
    // old packages
    const std::set<String> skipped_packages{
        "org.sw.demo.google.grpc.third_party.upb.utf8_range-1.54.2",
        "org.sw.demo.google.Orbit.third_party.multicore-1.52.0",
        "org.sw.demo.google.tesseract.wordlist2dawg-4.1.2",
        "org.sw.demo.kcat.tools.bsincgen-1.20.1",
        "org.sw.demo.malaterre.GDCM.uuid-3.0.22",
        "org.sw.demo.ocornut.imgui.backend.marmalade-1.85.0",
        "org.sw.demo.openexr.IlmImf-2.5.",
        "org.sw.demo.qtproject.qt.base.entrypoint-6.3.0",
        "org.sw.demo.qtproject.qt.declarative.tools.shared-5.15.0.1",
        "org.sw.demo.qtproject.qt.labs.vstools.natvis-3.0.1",
    };
    for (auto &&[p, vp] : new_pkgs) {
        auto pkg = p.toString();
        if (skipped_packages.contains(pkg)) {
            continue;
        }
#ifdef _WIN32
        // systemd repo contains NTFS-invalid files
        if (pkg.starts_with("org.sw.demo.systemd")) {
            continue;
        }
#endif
        auto &&v = vp.first;
        auto &&prefix = vp.second;
        LOG_INFO(logger, "sw uri --silent sw:upload " << pkg << " " << v.toString() << " " << prefix);

        if (swctx.getOptions().options_service.run) {
            primitives::Command c;
            c.arguments = {"sw", "uri", "--silent", "sw:upload", pkg, v.toString(), std::to_string(prefix)};
            c.out.inherit = true;
            c.err.inherit = true;
            std::error_code ec;
            c.execute(ec);
            LOG_INFO(logger, "");
        }
    }
}

SUBCOMMAND_DECL(service)
{
    boost::replace_all(getOptions().options_service.command, "-", "_");
#define CMD(f, ...)                                 \
    if (getOptions().options_service.command == #f) \
    {                                               \
        f(*this);                                   \
        return;                                     \
    }
    CMD(update_packages)
    else {
        throw SW_RUNTIME_ERROR("unknown command");
    }
}
