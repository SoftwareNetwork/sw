// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2019-2020 Egor Pugin <egor.pugin@gmail.com>

#include "mainwindow.h"

#include <cl.llvm.h>
#include "sw_context.h"

#include <sw/client/common/main.h>

#include <qapplication.h>
#include <qglobal.h>
#include <qmessagebox.h>
#include <qthread.h>
#include <QtWin>

#include <primitives/sw/settings_program_name.h>

#ifdef QT_STATIC
#include <QtPlugin>
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin)
Q_IMPORT_PLUGIN(QWindowsVistaStylePlugin)
#endif

#include <time.h>

#ifdef WIN32
#include <Windows.h>
#endif

void win32_hacks();

int main(int argc, char *argv[])
{
    // cli mode
    if (argc > 1)
    {
        StartupData sd(argc, argv);
        sd.program_short_name = SHORT_PROGRAM_NAME;
        return sd.run();
    }

    win32_hacks();
    qsrand(time(0));

    QThread t(0);
    QApplication a(argc, argv);

    auto hIcon = (HICON)LoadImage(GetModuleHandle(nullptr), MAKEINTRESOURCE(100), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_LOADTRANSPARENT);
    if (hIcon)
        QApplication::setWindowIcon(QIcon(QtWin::fromHICON(hIcon)));
    ::DestroyIcon(hIcon);

    try
    {
        ClOptions cloptions;
        Options options(cloptions);
        SwGuiContext swctx(options);
        MainWindow w(swctx);
        w.show();
        return a.exec();
    }
    catch (std::exception &e)
    {
        QMessageBox::critical(0, "Error", e.what(), "Ok");
    }
    catch (...)
    {
        QMessageBox::critical(0, "Error", "Unknown error.", "Ok");
    }
    return 1;
}

void win32_hacks()
{
#ifdef WIN32
    //SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
#endif
}

EXPORT_FROM_EXECUTABLE
std::string getProgramName()
{
    return PACKAGE_NAME_CLEAN;
}
