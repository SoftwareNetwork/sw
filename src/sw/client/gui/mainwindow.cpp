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
#include <qcheckbox.h>
#include <qcombobox.h>
#include <qgroupbox.h>
#include <qheaderview.h>
#include <qlabel.h>
#include <qpushbutton.h>
#include <qspinbox.h>
#include <qstylepainter.h>
#include <qtabbar.h>
#include <qtableview.h>
#include <qtextedit.h>

#include <sw/client/common/common.h>
#include <sw/client/common/generator/generator.h>
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
    //setWindowTitle("SW GUI"); // SW is present in icon
    setWindowTitle("GUI");

    setupUi();

    //resize(minimumSizeHint());
    resize(600, 600);
}

void MainWindow::setupUi()
{
    swctx = createSwContext();

    auto mainLayout = new QHBoxLayout;
    auto t = new TabWidget;
    //t->setDocumentMode(true);
    //t->setAutoFillBackground(true);

    //
    auto ctrlLayout = new QVBoxLayout;
    {
        auto topL = new QHBoxLayout;
        auto botL = new QVBoxLayout;
        ctrlLayout->addLayout(topL);
        ctrlLayout->addLayout(botL);

        auto topLL = new QVBoxLayout;
        auto topRL = new QVBoxLayout;
        topL->addLayout(topLL);
        topL->addLayout(topRL);

        // configuration
        {
            auto gb = new QGroupBox("Configuration");
            topLL->addWidget(gb);
            QVBoxLayout *gbl = new QVBoxLayout;
            gbl->addWidget(new QCheckBox("Debug"));
            auto cb = new QCheckBox("Release");
            cb->setChecked(true);
            gbl->addWidget(cb);
            gbl->addWidget(new QCheckBox("Release With Debug Information"));
            gbl->addWidget(new QCheckBox("Minimal Size Release"));
            gbl->addStretch(1);
            gb->setLayout(gbl);
        }

        // shared/static
        {
            auto gb = new QGroupBox("Linking");
            topLL->addWidget(gb);
            QVBoxLayout *gbl = new QVBoxLayout;
            auto cb = new QCheckBox("Dynamic (.dll)");
            cb->setChecked(true);
            gbl->addWidget(cb);
            gbl->addWidget(new QCheckBox("Static (.lib)"));
            gbl->addStretch(1);
            gb->setLayout(gbl);
        }

        // mt/md
        {
            auto gb = new QGroupBox("Runtime");
            topLL->addWidget(gb);
            QVBoxLayout *gbl = new QVBoxLayout;
            auto cb = new QCheckBox("Dynamic (MD/MDd)");
            cb->setChecked(true);
            gbl->addWidget(cb);
            gbl->addWidget(new QCheckBox("Static (MT/MTd)"));
            gbl->addStretch(1);
            gb->setLayout(gbl);
        }

        // arch
        {
            auto gb = new QGroupBox("Architecture");
            topLL->addWidget(gb);
            QVBoxLayout *gbl = new QVBoxLayout;
            // basic list
            gbl->addWidget(new QCheckBox("x86"));
            auto cb = new QCheckBox("x64");
            cb->setChecked(true);
            gbl->addWidget(cb);
            gbl->addWidget(new QCheckBox("arm"));
            gbl->addWidget(new QCheckBox("aarch64"));
            gbl->addStretch(1);
            gb->setLayout(gbl);
        }

        // compilers
        {
            auto gb = new QGroupBox("Compiler");
            topRL->addWidget(gb);
            QVBoxLayout *gbl = new QVBoxLayout;
            auto cls = list_compilers(*swctx);
            for (auto &cl : cls)
            {
                auto gb = new QGroupBox(cl.name.c_str());
                gbl->addWidget(gb);
                QVBoxLayout *gbl = new QVBoxLayout;
                for (auto &[pkg,_] : cl.releases)
                    gbl->addWidget(new QCheckBox(pkg.getVersion().toString().c_str()));
                for (auto &[pkg,_] : cl.prereleases)
                    gbl->addWidget(new QCheckBox(pkg.getVersion().toString().c_str()));
                gbl->addStretch(1);
                gb->setLayout(gbl);
            }
            gbl->addStretch(1);
            gb->setLayout(gbl);
        }

        ctrlLayout->addWidget(new QPushButton("Build"));
        ctrlLayout->addWidget(new QPushButton("Test"));

        // generators
        {
            auto gb = new QGroupBox("Generators");
            ctrlLayout->addWidget(gb);
            QVBoxLayout *gbl = new QVBoxLayout;
            //
            auto cb = new QComboBox();
            cb->setEditable(false);
            for (GeneratorType g = (GeneratorType)0; g < GeneratorType::Max; ((int&)g)++)
            {
                cb->addItem(toString(g).c_str(), (int)g);
            }
            cb->model()->sort(0);

#ifdef _WIN32
            int index = cb->findData((int)GeneratorType::VisualStudio);
            if (index != -1)
                cb->setCurrentIndex(index);
#endif
            gbl->addWidget(cb);
            gbl->addWidget(new QPushButton("Generate"));

            //
            gbl->addStretch(1);
            gb->setLayout(gbl);
        }

        ctrlLayout->addStretch(1);
    }

    auto ctrl = new QWidget;
    ctrl->setLayout(ctrlLayout);

    //
    t->addTab(ctrl, "Control");
    t->addTab(new QWidget, "Search");

    //
    auto add_packages_tab = [this, t](const String &name, auto &db)
    {
        auto v = new QTableView;

        auto m = new PackagesModel(db, true);
        v->setModel(m);
        v->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

        auto l = new QVBoxLayout;
        auto te = new QLineEdit;
        te->setPlaceholderText("Search Software...");
        connect(te, &QLineEdit::textChanged, [te, m]()
        {
            m->setFilter(te->text());
        });
        l->addWidget(te);
        l->addWidget(v);

        auto w = new QWidget;
        w->setLayout(l);
        auto idx = t->addTab(w, name.c_str());
        connect(t, &TabWidget::currentChanged, [idx, m](int i)
        {
            if (i != idx)
                return;
            m->init();
        });
    };

    add_packages_tab("Installed Packages", swctx->getLocalStorage().getPackagesDatabase());
    for (auto rs : swctx->getRemoteStorages())
    {
        if (auto s1 = dynamic_cast<sw::StorageWithPackagesDatabase *>(rs))
            add_packages_tab("Remote Packages: " + rs->getName(), s1->getPackagesDatabase());
    }

    //
    auto add_text_tab = [t](const String &name, const String &text)
    {
        auto te = new QTextEdit();
        te->setPlainText(text.c_str());
        te->setReadOnly(true);
        t->addTab(te, name.c_str());
    };

    add_text_tab("List of Predefined Targets", list_predefined_targets(*swctx));
    add_text_tab("List of Programs", list_programs(*swctx));

    //
    auto setLayout = new QVBoxLayout;
    {
        // -j
        {
            setLayout->addWidget(new QLabel("Number of threads"));
            auto sb = new QSpinBox();
            sb->setMinimum(1);
            sb->setMaximum(std::thread::hardware_concurrency() + 4);
            sb->setValue(std::thread::hardware_concurrency());
            setLayout->addWidget(sb);
        }

        setLayout->addStretch(1);
    }

    auto set = new QWidget;
    set->setLayout(setLayout);
    t->addTab(set, "Settings");

    //
    mainLayout->addWidget(t);

    auto centralWidget = new QWidget;
    centralWidget->setLayout(mainLayout);
    setCentralWidget(centralWidget);
}
