// SW C API

#ifndef _SW_MANAGER_C_H_
#define _SW_MANAGER_C_H_

#include <sw/sw-c.h>

enum
{
    SW_PATH_ROOT   =   0,
    SW_PATH_SDIR,
    SW_PATH_BDIR,
};

const char *sw_get_package_path(const sw_context_t *ctx, const char *package, int type = SW_PATH_ROOT);

// dl, build
int sw_build_package(const sw_context_t *ctx, const char *package, const char *config_file = 0);
int sw_download_package(const sw_context_t *ctx, const char *package);
int sw_install_package(const sw_context_t *ctx, const char *package, const char *config_file = 0);
int sw_run_package(const sw_context_t *ctx, const char *package, const char *config_file = 0);
int sw_test_package(const sw_context_t *ctx, const char *package, const char *config_file = 0);

#endif
