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

int main(int argc, char **argv)
{
    static cl::opt<String> loglevel("log-level", cl::init("INFO"));
    static cl::opt<path> dir("dir", cl::Required, cl::desc("Dir to store files"));
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
        auto &db = s2->getPackagesDatabase();
        auto ppaths = db.getMatchingPackages();
        for (auto &p : ppaths)
        {
            auto versions = db.getVersionsForPackage(p);
            for (auto &v : versions)
            {
                sw::PackageId pkgid{ p,v };
                auto f = s2->getFile(pkgid, sw::StorageFileType::SourceArchive);
                if (f->copy(dir / "1.tar.gz"))
                {
                    LOG_DEBUG(logger, "Download ok for: " + pkgid.toString() + ": source archive");
                }
                else
                {
                    LOG_WARN(logger, "Download failed for: " + pkgid.toString() + ": source archive");
                }
            }
        }
    }

    return 0;
}

EXPORT_FROM_EXECUTABLE
std::string getProgramName()
{
    return PACKAGE_NAME_CLEAN;
}
