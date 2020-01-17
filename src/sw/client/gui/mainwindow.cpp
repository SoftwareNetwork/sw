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

#include "mainwindow.h"

#include "packages_model.h"

#include <qboxlayout.h>
#include <qheaderview.h>
#include <qlabel.h>
#include <qstylepainter.h>
#include <qtabbar.h>
#include <qtableview.h>
#include <qtextedit.h>

#include <sw/client/common/common.h>
#include <sw/manager/storage.h>

class TabBar : public QTabBar
{
public:
    QSize tabSizeHint(int index) const
    {
        QSize s = QTabBar::tabSizeHint(index);
        s.transpose();
        return s;
    }

protected:
    void paintEvent(QPaintEvent *)
    {
        QStylePainter painter(this);
        QStyleOptionTab opt;

        for (int i = 0; i < count(); i++)
        {
            initStyleOption(&opt, i);
            painter.drawControl(QStyle::CE_TabBarTabShape, opt);
            painter.save();

            QSize s = opt.rect.size();
            s.transpose();
            QRect r(QPoint(), s);
            r.moveCenter(opt.rect.center());
            opt.rect = r;

            QPoint c = tabRect(i).center();
            painter.translate(c);
            painter.rotate(90);
            painter.translate(-c);
            painter.drawControl(QStyle::CE_TabBarTabLabel,opt);
            painter.restore();
        }
    }
};

class TabWidget : public QTabWidget
{
public:
    TabWidget(QWidget *parent = nullptr)
        : QTabWidget(parent)
    {
        setTabBar(new TabBar);
        setTabPosition(QTabWidget::West);
    }
};

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("SW GUI");

    setupUi();

    //resize(minimumSizeHint());
    resize(800, 600);
}

void MainWindow::setupUi()
{
    ctx = createSwContext();

    auto mainLayout = new QHBoxLayout;
    auto t = new TabWidget;

    t->addTab(new QWidget, "Search");
    t->addTab(new QWidget, "Control");

    auto add_packages_tab = [this, t](const String &name, auto &db)
    {
        auto v = new QTableView;
        auto m = new PackagesModel(db, true);
        v->setModel(m);
        v->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
        auto idx = t->addTab(v, name.c_str());
        connect(t, &TabWidget::currentChanged, [idx, m](int i)
        {
            if (i != idx)
                return;
            m->init();
        });
    };

    add_packages_tab("Installed Packages", ctx->getLocalStorage().getPackagesDatabase());
    for (auto rs : ctx->getRemoteStorages())
    {
        if (auto s1 = dynamic_cast<sw::StorageWithPackagesDatabase *>(rs))
            add_packages_tab("Remote Packages: " + rs->getName(), s1->getPackagesDatabase());
    }

    auto add_text_tab = [t](const String &name, const String &text)
    {
        auto te = new QTextEdit();
        te->setPlainText(text.c_str());
        te->setReadOnly(true);
        t->addTab(te, name.c_str());
    };

    add_text_tab("List of Predefined Targets", list_predefined_targets());
    add_text_tab("List of Programs", list_programs());

    mainLayout->addWidget(t);

    auto centralWidget = new QWidget;
    centralWidget->setLayout(mainLayout);
    setCentralWidget(centralWidget);
}
