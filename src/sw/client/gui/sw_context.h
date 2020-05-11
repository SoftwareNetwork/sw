/*
 * SW - Build System and Package Manager
 * Copyright (C) 2019-2020 Egor Pugin
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <sw/client/common/sw_context.h>
#include <qstring.h>

struct SwGuiContext : SwClientContext
{
    using Base = SwClientContext;

    SwGuiContext(const Options &options, const ClOptions &cloptions);

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
