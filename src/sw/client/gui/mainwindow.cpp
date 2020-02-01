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
#include <qfiledialog.h>
#include <qgroupbox.h>
#include <qheaderview.h>
#include <qlabel.h>
#include <qlineedit.h>
#include <qpushbutton.h>
#include <qspinbox.h>
#include <qstylepainter.h>
#include <qtabbar.h>
#include <qtableview.h>
#include <qtextedit.h>

#include <sw/client/common/common.h>
#include <sw/client/common/generator/generator.h>
#include <sw/manager/database.h>
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

MainWindow::MainWindow(sw::SwContext &swctx, QWidget *parent)
    : QMainWindow(parent)
    , swctx(swctx)
{
    //setWindowTitle("SW GUI"); // SW is present in icon
    setWindowTitle("GUI");

    setupUi();

    //resize(minimumSizeHint());
    resize(200, 200);
}

void MainWindow::setupUi()
{
    auto mainLayout = new QHBoxLayout;
    auto t = new TabWidget;
    //t->setDocumentMode(true);
    //t->setAutoFillBackground(true);

    //
    auto ctrlLayout = new QHBoxLayout;
    {
        auto left = new QVBoxLayout;
        auto middle = new QVBoxLayout;
        auto right = new QVBoxLayout;

        ctrlLayout->addLayout(left);
        ctrlLayout->addLayout(middle);
        ctrlLayout->addLayout(right);

        // configuration
        {
            auto gb = new QGroupBox("Configuration");
            middle->addWidget(gb);
            QVBoxLayout *gbl = new QVBoxLayout;
            gbl->addWidget(new QCheckBox("Debug"));
            gbl->addWidget(new QCheckBox("Minimal Size Release"));
            gbl->addWidget(new QCheckBox("Release With Debug Information"));
            auto cb = new QCheckBox("Release");
            cb->setChecked(true);
            gbl->addWidget(cb);
            gb->setLayout(gbl);
        }

        // shared/static
        {
            auto gb = new QGroupBox("Linking");
            middle->addWidget(gb);
            QVBoxLayout *gbl = new QVBoxLayout;
            auto cb = new QCheckBox("Dynamic (.dll)");
            cb->setChecked(true);
            gbl->addWidget(cb);
            gbl->addWidget(new QCheckBox("Static (.lib)"));
            gb->setLayout(gbl);
        }

        // mt/md
        {
            auto gb = new QGroupBox("Runtime");
            middle->addWidget(gb);
            QVBoxLayout *gbl = new QVBoxLayout;
            auto cb = new QCheckBox("Dynamic (MD/MDd)");
            cb->setChecked(true);
            gbl->addWidget(cb);
            gbl->addWidget(new QCheckBox("Static (MT/MTd)"));
            gb->setLayout(gbl);
        }

        // arch
        {
            auto gb = new QGroupBox("Architecture");
            middle->addWidget(gb);
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
            right->addWidget(gb);
            QVBoxLayout *gbl = new QVBoxLayout;
            auto cls = list_compilers(swctx);
            bool set = false;
            for (auto &cl : cls)
            {
                auto gb = new QGroupBox(cl.name.c_str());
                gbl->addWidget(gb);
                QVBoxLayout *gbl = new QVBoxLayout;
                gb->setLayout(gbl);
                for (auto &[pkg,_] : cl.releases)
                    gbl->addWidget(new QCheckBox(pkg.getVersion().toString().c_str()));
                if (!set && !cl.releases.empty())
                {
                    ((QCheckBox*)gb->children().back())->setChecked(true);
                    set = true;
                }
                for (auto &[pkg,_] : cl.prereleases)
                    gbl->addWidget(new QCheckBox(pkg.getVersion().toString().c_str()));
                gbl->addStretch(1);
            }
            gbl->addStretch(1);
            gb->setLayout(gbl);
        }

        // inputs
        {
            auto gb = new QGroupBox("Inputs");
            gb->setMinimumWidth(350);
            left->addWidget(gb, 1);
            QVBoxLayout *gbl = new QVBoxLayout;
            gb->setLayout(gbl);
            auto afile = new QPushButton("Add File");
            auto adir = new QPushButton("Add Directory");
            auto pkgcb = new QComboBox();
            auto apkg = new QPushButton("Add Package");
            gbl->addWidget(afile);
            gbl->addWidget(adir);
            gbl->addWidget(pkgcb);
            gbl->addWidget(apkg);

            connect(pkgcb, &QComboBox::currentTextChanged, [this, pkgcb]()
            {
                return;
                auto &rs = swctx.getRemoteStorages();
                if (rs.empty())
                    return;
                if (auto s1 = dynamic_cast<sw::StorageWithPackagesDatabase *>(rs[0]))
                {
                    pkgcb->clear();
                    auto ppaths = s1->getPackagesDatabase().getMatchingPackages(pkgcb->currentText().toStdString());
                    for (auto &ppath : ppaths)
                    {
                        auto vs = s1->getPackagesDatabase().getVersionsForPackage(ppath);
                        for (auto &v : vs)
                            pkgcb->addItem(sw::PackageId{ ppath, v }.toString().c_str());
                    }
                }
            });
            pkgcb->setAutoCompletion(true);
            pkgcb->setEditable(true);

            auto add_input = [gbl](const auto &s)
            {
                auto le = new QLineEdit(s);
                le->setEnabled(false);
                gbl->addWidget(le);
            };

            connect(apkg, &QPushButton::clicked, [add_input, pkgcb]()
            {
                add_input(pkgcb->currentText());
            });

            connect(afile, &QPushButton::clicked, [this, add_input]()
            {
                QFileDialog dialog(this);
                dialog.setFileMode(QFileDialog::ExistingFile);
                if (dialog.exec())
                    add_input(dialog.selectedFiles()[0]);
            });

            connect(adir, &QPushButton::clicked, [this, add_input]()
            {
                QFileDialog dialog(this);
                dialog.setFileMode(QFileDialog::Directory);
                if (dialog.exec())
                    add_input(dialog.selectedFiles()[0]);
            });

            gbl->addStretch(1);
        }

        left->addWidget(new QPushButton("Build"));
        left->addWidget(new QPushButton("Test"));

        // generators
        {
            auto gb = new QGroupBox("Generators");
            left->addWidget(gb);
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

        middle->addStretch(1);
        ctrlLayout->addStretch(1);
    }

    auto ctrl = new QWidget;
    ctrl->setLayout(ctrlLayout);
    t->addTab(ctrl, "General");

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

    add_packages_tab("Installed Packages", swctx.getLocalStorage().getPackagesDatabase());
    for (auto rs : swctx.getRemoteStorages())
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

    add_text_tab("List of Predefined Targets", list_predefined_targets(swctx));
    add_text_tab("List of Programs", list_programs(swctx));

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
