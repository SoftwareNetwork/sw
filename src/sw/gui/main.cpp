#include "mainwindow.h"

#include <qapplication.h>
#include <qglobal.h>
#include <qthread.h>

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
    win32_hacks();
    qsrand(time(0));

    QThread t(0);
    QApplication a(argc, argv);
    MainWindow w;
    w.show();

    return a.exec();
}

void win32_hacks()
{
#ifdef WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
#endif
}
