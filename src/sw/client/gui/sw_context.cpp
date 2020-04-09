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

#include "sw_context.h"

#include "logwindow.h"

#include <qthread.h>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "gui.sw_context");

void SwGuiContext::command_build()
{
    run_with_log("Build Log", [this]()
    {
        Base::command_build();
    });
}

void SwGuiContext::command_test()
{
    run_with_log("Test Log", [this]()
    {
        Base::command_test();
    });
}

void SwGuiContext::run_with_log(const QString &title, std::function<void(void)> f)
{
    auto w = new LogWindow(*this);
    w->setBaseSize({400,300});
    w->setWindowTitle(title);
    w->show();

    auto t = QThread::create([w, f]
    {
        try
        {
            f();
            LOG_INFO(logger, "Finished.");
        }
        catch (std::exception &e)
        {
            LOG_INFO(logger, e.what());
        }
        catch (...)
        {
            LOG_INFO(logger, "Unknown exception.");
        }
        w->stop();
    });
    t->start();
}
