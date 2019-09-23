#include "common.h"

#include <sw/driver/driver.h>
#include <sw/manager/settings.h>

#include <primitives/http.h>
#include <primitives/sw/cl.h>

static ::cl::opt<path> storage_dir_override("storage-dir");

static ::cl::opt<bool> curl_verbose("curl-verbose");
static ::cl::opt<bool> ignore_ssl_checks("ignore-ssl-checks");

std::unique_ptr<sw::SwContext> createSwContext()
{
    // load proxy settings early
    httpSettings.verbose = curl_verbose;
    httpSettings.ignore_ssl_checks = ignore_ssl_checks;
    httpSettings.proxy = sw::Settings::get_local_settings().proxy;

    auto swctx = std::make_unique<sw::SwContext>(storage_dir_override.empty() ? sw::Settings::get_user_settings().storage_dir : storage_dir_override);
    // TODO:
    // before default?
    //for (auto &d : drivers)
    //swctx->registerDriver(std::make_unique<sw::driver::cpp::Driver>());
    swctx->registerDriver("org.sw.sw.driver.cpp-0.3.1"s, std::make_unique<sw::driver::cpp::Driver>());
    //swctx->registerDriver(std::make_unique<sw::CDriver>(sw_create_driver));
    return swctx;
}
