// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2019-2020 Egor Pugin <egor.pugin@gmail.com>

#include "sw_context.h"

#include "logwindow.h"

#include <qcoreapplication.h>
#include <qmessagebox.h>
#include <qstatusbar.h>
#include <qthread.h>
#include <qtimer.h>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "gui.sw_context");

template <class F>
void exception_safe_call(F &&f, String *error = nullptr)
{
    try
    {
        f();
    }
    catch (std::exception &e)
    {
        if (error)
            *error = e.what();
    }
    catch (...)
    {
        if (error)
            *error = "Unknown exception.";
    }
}

SwGuiContext::SwGuiContext(const Options &options)
    : SwClientContext(options)
{
}

#define ADD_COMMAND(cmd, name)                \
    void SwGuiContext::command_##cmd()        \
    {                                         \
        if (check_running())                  \
            return;                           \
        String n = name " Log";               \
        n[0] = toupper(n[0]);                 \
        run_with_log(n.c_str(), [this]() {    \
            SwapAndRestore sr(running, true); \
            Base::command_##cmd();            \
        });                                   \
    }

ADD_COMMAND(build, "Build")
ADD_COMMAND(create, "Create")
ADD_COMMAND(generate, "Generate")
ADD_COMMAND(test, "Test")

void SwGuiContext::command_open()
{
    String error;
    exception_safe_call([this] {Base::command_open(); }, &error);
    if (!error.empty())
        QMessageBox::critical(0, 0, error.c_str());
}

void SwGuiContext::run_with_log(const QString &title, std::function<void(void)> f)
{
    // we are already working
    if (QThread::currentThread() != QCoreApplication::instance()->thread())
        return f();

    auto w = new LogWindow(*this);
    w->setMinimumSize({400,300});
    w->setWindowTitle(title);
    w->show();

    // first access to status bar must be in main (gui) thread
    w->statusBar()->showMessage("Starting...");
    auto timer = new QTimer(w);
    w->connect(timer, &QTimer::timeout, [w, i = 0]() mutable
    {
        auto sym = "/-\\|";
        w->statusBar()->showMessage(QString("Working...\t") + sym[i++ % 4]);
    });
    timer->start(250);

    auto t = QThread::create([w, f, timer]
    {
        w->tid = std::this_thread::get_id();
        LOG_INFO(logger, "Starting...");
        String error;
        exception_safe_call([f] {f(); }, &error);
        if (!error.empty())
            LOG_INFO(logger, error);
        LOG_INFO(logger, "Finished.");
        LOG_FLUSH();
        w->stopLogging();
        emit w->hideCancelButton();
        timer->stop(); // Timers cannot be stopped from another thread
        w->statusBar()->showMessage(w->cancelled ? "Cancelled." : "Finished.");
    });
    t->start();
}

bool SwGuiContext::check_running() const
{
    if (QThread::currentThread() != QCoreApplication::instance()->thread())
        return false;
    if (running)
        QMessageBox::warning(0, 0, "Operation is already in progress!");
    return running;
}
