// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2020 Egor Pugin <egor.pugin@gmail.com>

#include <sw/manager/database.h>
#include <sw/manager/package_database.h>
#include <sw/manager/settings.h>
#include <sw/manager/storage.h>
#include <sw/manager/storage_remote.h>
#include <sw/manager/sw_context.h>

#include <primitives/executor.h>
#include <primitives/http.h>
#include <primitives/sw/main.h>
#include <primitives/sw/cl.h>
#include <primitives/sw/settings_program_name.h>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "server.mirror");

void setup_log(const std::string &log_level)
{
    LoggerSettings log_settings;
    log_settings.log_level = log_level;
    log_settings.simple_logger = true;
    log_settings.print_trace = true;
    initLogger(log_settings);

    // first trace message
    LOG_TRACE(logger, "----------------------------------------");
    LOG_TRACE(logger, "Starting sw...");
}

int main(int argc, char *argv[])
{
    static cl::opt<String> loglevel("log-level", cl::init("INFO"));
    static cl::opt<path> dir("dir", cl::Required, cl::desc("Dir to store files"));
    // this probably must be read from specifications.json for this storage (as well as dir?)
    static cl::opt<String> path_format("path-format", cl::desc("Storage path format"), cl::init("{PHPF}/{FN}"));
    // filters:
    // - file size
    // - package path
    // - skip list (package path/ids)
    // - file type
    // - percentage of suitable packages

    cl::ParseCommandLineOptions(argc, argv);

    // init
    setup_log(loglevel);
    primitives::http::setupSafeTls();

    Executor e(select_number_of_threads());
    getExecutor(&e);

    //
    sw::SwManagerContext swctx(sw::Settings::get_user_settings().storage_dir, true);
    for (auto &s : swctx.getRemoteStorages())
    {
        auto s2 = dynamic_cast<sw::StorageWithPackagesDatabase *>(s);
        if (!s2)
            continue;

        sw::PackageIdSet pkgs;
        auto &db = s2->getPackagesDatabase();
        auto ppaths = db.getMatchingPackages();
        for (auto &p : ppaths)
        {
            auto versions = db.getVersionsForPackage(p);
            for (auto &v : versions)
                pkgs.insert({ p,v });
        }

        LOG_DEBUG(logger, "Total packages: " << pkgs.size());

        auto &e = getExecutor();
        std::atomic_size_t i = 0;
        Futures<void> jobs;
        for (auto &pkg : pkgs)
        {
            sw::Package pkgid{ *s2, pkg };
            auto dst = dir / pkgid.formatPath(path_format);
            if (fs::exists(dst))
                continue;
            jobs.emplace_back(e.push([dst, pkgid = std::move(pkgid), s2, &i, &jobs]()
            {
                auto bak = path(dst) += ".bak";
                // maybe we should create target storage?
                // SwManagerContext or just Directories to get pkg dir and to keep standard layout
                // and the operation will download from storage to storage
                auto f = s2->getFile(pkgid, sw::StorageFileType::SourceArchive);
                if (f->copy(bak))
                {
                    fs::rename(bak, dst);
                    LOG_DEBUG(logger, "[" << ++i << "/" << jobs.size() << "] Download ok for: " + pkgid.toString() + ": source archive");
                }
                else
                {
                    LOG_WARN(logger, "[" << ++i << "/" << jobs.size() << "] Download failed for: " + pkgid.toString() + ": source archive");
                }
            }));
        }
        LOG_DEBUG(logger, "Total files to download: " << jobs.size());
        waitAndGet(jobs);
    }

    return 0;
}
