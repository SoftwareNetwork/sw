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

namespace {
bool has_prefix;
}

struct http_request_cache {
    struct data {
        long http_code = 0;
        String response;
        //       new version                old version
        std::map<sw::Version, std::multimap<sw::Version, sw::PackageId>> packages;
    };
    std::map<std::string, data> new_versions;

    data &test_url1(auto &&key_url, std::string additional_url, HttpRequest &request) {
        auto &source_id = key_url; // d.source has real tag so it is now useful
        if (new_versions[source_id].http_code == 0) {
            request.connect_timeout = 1;
            request.url = key_url + additional_url;
            try
            {
                auto resp = url_request(request);
                new_versions[source_id].http_code = resp.http_code;
                new_versions[source_id].response = resp.response;
            }
            catch (std::exception &)
            {
            }
        }
        return new_versions[source_id];
    }
    data &test_url(auto &&key_url, std::string additional_url = {}) {
        HttpRequest request{httpSettings};
        return test_url1(key_url, additional_url, request);
    }
    void post_process(auto &&swctx, auto &&pdb) {
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
            "org.sw.demo.malaterre.GDCM.uuid-3.0.",
            "org.sw.demo.ocornut.imgui.backend.marmalade-1.85.0",
            "org.sw.demo.openexr.IlmImf-2.5.",
            "org.sw.demo.qtproject.qt.base.entrypoint-6.3.0",
            "org.sw.demo.qtproject.qt.declarative.tools.shared-5.15.0.1",
            "org.sw.demo.qtproject.qt.labs.vstools.natvis-3.0.1",
        };
        for (auto &&[p, vp] : new_pkgs) {
            auto pkg = p.toString();
            if (std::ranges::any_of(skipped_packages, [&](auto &&v){ return pkg.starts_with(v); })) {
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
};

auto extract_version_from_git_tag(std::string &ver, auto &&tag) {
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
        while (!ver.empty() && !isalpha(ver[0]) && !isdigit(ver[0])) {
            ver = ver.substr(1);
        }
        if (!ver.empty() && isdigit(ver[0])) {
            continue;
        }
        // skip pre releases
        if (std::ranges::find_if(ver, isalpha) != ver.end())
        {
            //LOG_WARN(logger, "pre release? " << line.substr(line.rfind('/') + 1));
            //LOG_WARN(logger, "ver: " << ver);
            auto p = tag.rfind(ver);
            bool good_ending = p != -1 && p + ver.size() == tag.size();
            if (!has_prefix && !good_ending) {
                numbers.clear();
                break;
            }
        }
    }
    if (numbers.empty()) {
        return false;
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
    return true;
}

auto try_extract_new_ver_from_git_tags(auto &&lines, auto &&tag, auto &&maxver, auto &&d, auto &&cache_record, auto &&pkgid) {
    for (auto &&line : lines) {
        auto ver = line.substr(line.rfind('/') + 1);
        if (!extract_version_from_git_tag(ver, tag)) {
            continue;
        }
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
                            LOG_WARN(logger, "cant find " << tofind << " in " << git->tag);
                            break;
                        }
                        git->tag = git->tag.substr(0, pos) + std::to_string(v[i]) + git->tag.substr(pos + tofind.size());
                        pos += tofind.size();
                        //LOG_INFO(logger, git->tag);
                    }
                    if (line.ends_with("refs/tags/" + git->tag)) {
                        cache_record.packages[v].insert({maxver, pkgid});
                        LOG_INFO(logger, "new version: " << pkgid.toString() << ": " << v.toString());
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
                            cache_record.packages[v].insert({maxver, pkgid});
                            LOG_INFO(logger, "new version: " << pkgid.toString() << ": " << v.toString());
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
                            LOG_INFO(logger, "tag fixed: " << pkgid.toString() << ": " << v.toString());
                        } else {
                            if (maxver.getPatch() == 0) {
                                auto verstring = maxver.toString();
                                verstring.resize(verstring.size() - 2); // remove .0
                                auto source = d.source;
                                boost::replace_all(source, verstring, "{M}.{m}{po}");
                                auto s = sw::source::load(nlohmann::json::parse(source));
                                auto git = dynamic_cast<primitives::source::Git *>(s.get());
                                if (apply(git)) {
                                    LOG_INFO(logger, "tag fixed: " << pkgid.toString() << ": " << v.toString());
                                } else {
                                    LOG_DEBUG(logger, "tag check error: " << pkgid.toString() << ": " << v.toString());
                                }
                            } else {
                                LOG_DEBUG(logger, "tag check error: " << pkgid.toString() << ": " << v.toString());
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

void update_packages(SwClientContext &swctx) {
    http_request_cache cache;
    auto &s = *swctx.getContext().getRemoteStorages().at(0);
    auto &rs = dynamic_cast<sw::RemoteStorage&>(s);
    auto &pdb = rs.getPackagesDatabase();
    String prefix;
    // prefix = "org.sw.demo.c_ares";
    prefix = "org.sw.demo.";
    has_prefix = !swctx.getOptions().options_service.args.empty();
    if (has_prefix) {
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
        auto &&pkgid = resolved.begin()->second;
        auto &&d = pdb.getPackageData(pkgid);
        if (d.source.empty()) {
            LOG_INFO(logger, "empty source: " << pkgid.toString());
            continue;
        }
        auto s = sw::source::load(nlohmann::json::parse(d.source));
        if (s->getType() != primitives::source::SourceType::Git) {
            if (s->getType() == primitives::source::SourceType::RemoteFile)
            {
                auto remote = dynamic_cast<primitives::source::RemoteFile *>(s.get());
                LOG_INFO(logger, "remote: " << remote->url);
                auto &source_id = remote->url; // d.source has real tag so it is now useful
                HttpRequest request{httpSettings};
                request.timeout = 1;
                if (auto &ret = cache.test_url1(source_id, {}, request); ret.http_code != 200)
                {
                    LOG_WARN(logger, "http " << ret.http_code << ": " << resolved.begin()->second.toString());
                    continue;
                }
                // increment from level() to 1 numbers (for 1..3)
                // test
                // set to zero and --level
                // add everything!
                continue;
            }
            continue;
        }
        //continue;
        auto git = dynamic_cast<primitives::source::Git*>(s.get());
        if (git->tag.empty()) {
            continue;
        }
        auto &source_id = git->url; // d.source has real tag so it is now useful
        auto &cache_record = cache.test_url(source_id, "/info/refs?service=git-upload-pack");
        if (cache_record.http_code != 200)
        {
            LOG_WARN(logger, "http " << cache_record.http_code << ": " << pkgid.toString());
            continue;
        }
        auto lines = split_lines(cache_record.response) | std::views::filter([](auto &&line)
                                                                                              { return line.contains("refs/tags/") && !line.contains("^"); });
        try_extract_new_ver_from_git_tags(lines, git->tag, maxver, d, cache_record, pkgid);
    }
    cache.post_process(swctx, pdb);
}

struct package_updater {
    http_request_cache cache;
    sw::Version maxver;

    void update(SwClientContext &swctx) {
        auto &s = *swctx.getContext().getRemoteStorages().at(0);
        auto &rs = dynamic_cast<sw::RemoteStorage &>(s);
        auto &pdb = rs.getPackagesDatabase();

        String prefix;
        //prefix = "org.sw.demo.amazon.awslabs.crt_cpp";
        prefix = "org.sw.demo.";
        has_prefix = !swctx.getOptions().options_service.args.empty();
        if (has_prefix) {
            prefix = swctx.getOptions().options_service.args[0];
        }
        auto all_pkgs = pdb.getMatchingPackages(prefix);
        for (int pkgid = 0; auto &&ppath : all_pkgs) {
            LOG_INFO(logger, "[" << ++pkgid << "/" << all_pkgs.size() << "] " << ppath.toString());
            auto versions = pdb.getVersionsForPackage(ppath);
            if (versions.empty() || versions.rbegin()->isBranch()) {
                continue;
            }
            maxver = *versions.rbegin();
            sw::UnresolvedPackages pkgs;
            pkgs.insert({ppath, maxver});
            sw::UnresolvedPackages upkgs;
            auto &&resolved = pdb.resolve(pkgs, upkgs);
            auto &&pkgid = resolved.begin()->second;
            auto &&d = pdb.getPackageData(pkgid);
            if (d.source.empty())
            {
                LOG_DEBUG(logger, "empty source: " << pkgid.toString());
                continue;
            }
            update(d, pkgid);
        }
        cache.post_process(swctx, pdb);
    }
    void update(const sw::PackageData &d, auto &&pkgid) {
        auto s = sw::source::load(nlohmann::json::parse(d.source));
        if (s->getType() == primitives::source::SourceType::Git) {
            auto git = dynamic_cast<primitives::source::Git *>(s.get());
            update(*git, d, pkgid);
        } else if (s->getType() == primitives::source::SourceType::RemoteFile) {
            auto rf = dynamic_cast<primitives::source::RemoteFile *>(s.get());
            update(*rf, d, pkgid);
        } else if (s->getType() == primitives::source::SourceType::RemoteFiles) {
            LOG_WARN(logger, "unsupported source");
        } else {
            LOG_WARN(logger, "unsupported source");
        }
    }
    static auto get_next_versions(const sw::Version &base) {
        std::set<sw::Version> v;
        auto ins = [&](auto &&ver) {return *v.insert(ver).first;};
        auto nextver = ins(base);
        auto level = base.getLevel();
        // 4 here
        nextver = ins(nextver.getNextVersion(level));
        nextver = ins(nextver.getNextVersion(level));
        nextver = ins(nextver.getNextVersion(level));
        nextver = ins(nextver.getNextVersion(level));
        //
        while (1) {
            auto v2 = nextver;
            v2[--level] = 0;
            if (level == 0) {
                break;
            }
            nextver = ins(v2);
            nextver = ins(nextver.getNextVersion(level));
            nextver = ins(nextver.getNextVersion(level));
            nextver = ins(nextver.getNextVersion(level));
            nextver = ins(nextver.getNextVersion(level));
            nextver = v2;
        }
        v.erase(base);
        return v;
    }
    void update(primitives::source::Git git, auto &&d, auto &&pkgid) {
        if (git.tag.empty()) {
            return;
        }
        auto &source_id = git.url; // d.source has real tag so it is now useful
        auto &cache_record = cache.test_url(source_id, "/info/refs?service=git-upload-pack");
        if (cache_record.http_code != 200) {
            LOG_WARN(logger, "http " << cache_record.http_code);
            return;
        }
        auto lines = split_lines(cache_record.response)
            | std::views::filter([](auto &&line){return line.contains("refs/tags/") && !line.contains("^");});
        std::vector common_git_tags{
            "{v}"s,
            "{M}.{m}"s,
            "{M}.{m}{po}"s,
            "{M}.{m}.{p}.{to}"s,
        };
        auto sz = common_git_tags.size();
        while (sz--) {
            common_git_tags.push_back("v"s + common_git_tags[sz]);
        }
        primitives::source::Git git2{"https//nonempty.url.com/"s, "nonempty"s};
        bool added{};
        auto newversions = get_next_versions(maxver);
        for (auto &&v : newversions) {
            for (auto &&t : common_git_tags) {
                git2.tag = t;
                git2.applyVersion(v);
                auto it = std::ranges::find_if(lines, [&](auto &&line)
                                     { return line.ends_with("refs/tags/"s + git2.tag); });
                if (it != std::ranges::end(lines) && v > maxver) {
                    cache_record.packages[v].insert({maxver, pkgid});
                    added = true;
                }
            }
        }
        if (!added) {
            try_extract_new_ver_from_git_tags(lines, git.tag, maxver, d, cache_record, pkgid);
        }
    }
    void update(primitives::source::RemoteFile rf, auto &&d, auto &&pkgid) {
        //
    }
};

void update_packages2(SwClientContext &swctx)
{
    package_updater u;
    u.update(swctx);

    /*struct data
    {
        long http_code = 0;
        String response;
        //       new version                old version
        std::map<sw::Version, std::multimap<sw::Version, sw::PackageId>> packages;
    };
    std::map<std::string, data> new_versions;
    for (int pkgid = 0; auto &&ppath : all_pkgs)
    {
        auto s = sw::source::load(nlohmann::json::parse(d.source));
        if (s->getType() == primitives::source::SourceType::Git)
        {
            auto git = dynamic_cast<primitives::source::Git *>(s.get());
            if (git->tag.empty())
            {
                continue;
            }
            auto &source_id = git->url; // d.source has real tag so it is now useful
            if (new_versions[source_id].http_code == 0)
            {
                HttpRequest request{httpSettings};
                request.url = git->url + "/info/refs?service=git-upload-pack";
                try
                {
                    auto resp = url_request(request);
                    new_versions[source_id].http_code = resp.http_code;
                    new_versions[source_id].response = resp.response;
                }
                catch (std::exception &)
                {
                }
            }
            if (new_versions[source_id].http_code != 200)
            {
                LOG_WARN(logger, "http " << new_versions[source_id].http_code << ": " << resolved.begin()->second.toString());
                continue;
            }

            auto v = maxver;
            std::set<sw::Version> versions2;
            auto level = v.getLevel();
            for (int i = 1; i <= level; ++i)
            {
                versions2.insert(v.getNextVersion(i));
            }

            for (auto &&v : versions2)
            {
                LOG_INFO(logger, "123: " << v.toString());
            }
            //std::ranges::contains();
        }
        else
        {
            LOG_DEBUG(logger, "unknown source: " << resolved.begin()->second.toString() << ": " << d.source);
            continue;
        }
    }
    LOG_INFO(logger, "\ncommand list\n");
    std::map<sw::PackageId, std::pair<sw::Version, int>> new_pkgs;
    for (auto &&[_, n] : new_versions)
    {
        if (n.packages.empty())
        {
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
    for (auto &&[p, vp] : new_pkgs)
    {
        auto pkg = p.toString();
        if (std::ranges::any_of(skipped_packages, [&](auto &&v)
                                { return pkg.starts_with(v); }))
        {
            continue;
        }
#ifdef _WIN32
        // systemd repo contains NTFS-invalid files
        if (pkg.starts_with("org.sw.demo.systemd"))
        {
            continue;
        }
#endif
        auto &&v = vp.first;
        auto &&prefix = vp.second;
        LOG_INFO(logger, "sw uri --silent sw:upload " << pkg << " " << v.toString() << " " << prefix);

        if (swctx.getOptions().options_service.run)
        {
            primitives::Command c;
            c.arguments = {"sw", "uri", "--silent", "sw:upload", pkg, v.toString(), std::to_string(prefix)};
            c.out.inherit = true;
            c.err.inherit = true;
            std::error_code ec;
            c.execute(ec);
            LOG_INFO(logger, "");
        }
    }*/
}

void update_packages3(SwClientContext &swctx)
{
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
    CMD(update_packages2)
    CMD(update_packages3)
    else {
        throw SW_RUNTIME_ERROR("unknown command");
    }
}
