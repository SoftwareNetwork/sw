// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2019-2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include <sw/client/common/sw_context.h>
#include <qstring.h>

struct SwGuiContext : SwClientContext
{
    using Base = SwClientContext;

    SwGuiContext(const Options &options);

    void command_build() override;
    void command_create() override;
    void command_generate() override;
    void command_open() override;
    void command_test() override;

private:
    bool running = false;

    void run_with_log(const QString &title, std::function<void(void)> f);
    bool check_running() const;
};
