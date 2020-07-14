// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2019-2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include <qabstractitemmodel.h>
#include <qmainwindow.h>

#define SHORT_PROGRAM_NAME "swgui"

struct SwGuiContext;
namespace sw { struct PackagesDatabase; }

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    MainWindow(SwGuiContext &swctx, QWidget *parent = 0);

private:
    SwGuiContext &swctx;
    sw::PackagesDatabase *remote_db = nullptr;

    void setupUi();

    void setupGeneral(QWidget *parent);
    void setupConfiguration(QWidget *parent);

    void createMenus();
};
