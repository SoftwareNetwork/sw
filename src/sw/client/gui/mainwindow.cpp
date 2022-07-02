// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2019-2020 Egor Pugin <egor.pugin@gmail.com>

#include "mainwindow.h"

#include "packages_model.h"
#include "settingswindow.h"
#include "stdvectoredit.h"
#include "sw_context.h"

#include <qaction.h>
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
#include <qscrollarea.h>
#include <qmenu.h>
#include <qmenubar.h>
#include <qmessagebox.h>
#include <qthread.h>
#include <qscrollbar.h>

#include <primitives/git_rev.h>
#include <primitives/sw/settings_program_name.h>
#include <sw/client/common/generator/generator.h>
#include <sw/client/common/commands.h>
#include <sw/client/common/self_upgrade.h>
#include <sw/manager/package_database.h>
#include <sw/manager/storage.h>

#include "cl_helper.h"
#include <cl.llvm.qt.inl>

#include <boost/format.hpp>
#include <boost/log/common.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/trivial.hpp>

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

MainWindow::MainWindow(SwGuiContext &swctx, QWidget *parent)
    : QMainWindow(parent)
    , swctx(swctx)
{
    // make switchable
    for (auto rs : swctx.getContext().getRemoteStorages())
    {
        if (auto s1 = dynamic_cast<sw::StorageWithPackagesDatabase *>(rs))
        {
            remote_db = &s1->getPackagesDatabase();
            break;
        }
    }

    setupUi();
    //resize(minimumSizeHint());
    //resize(200, 200);
    setMinimumSize(850, 500);
}

void MainWindow::setupUi()
{
    setWindowTitle("SW GUI");

    createMenus();

    auto mainLayout = new QHBoxLayout;
    auto t = new TabWidget;
    //t->setDocumentMode(true);
    //t->setAutoFillBackground(true);

    // General
    auto ctrl = new QWidget;
    setupGeneral(ctrl);
    t->addTab(ctrl, "General");

    // Configuration
    auto cfg = new QWidget;
    setupConfiguration(cfg);
    t->addTab(cfg, "Configuration");

    // create new project
    auto cnp = new QWidget;
    t->addTab(cnp, "Create New Project");

    // create
    {
        auto l = new QVBoxLayout;
        cnp->setLayout(l);

        l->addWidget(new QLabel("Directory"));
        auto l2 = new QHBoxLayout;
        l->addLayout(l2);
        auto le = new QLineEdit();
        l2->addWidget(le);

        connect(le, &QLineEdit::textChanged, [this](const QString &t)
        {
            swctx.getOptions().options_create.create_type = "project";
            swctx.getOptions().options_create.project_directory = t.toStdString();
        });

        auto sd = new QPushButton("Select");
        l2->addWidget(sd);

        connect(sd, &QPushButton::clicked, [this, le]()
        {
            QFileDialog dialog(this);
            dialog.setFileMode(QFileDialog::Directory);
            if (dialog.exec())
                le->setText(dialog.selectedFiles()[0]);
        });

        l->addWidget(new QLabel("Template"));
        auto cb = new QComboBox();
        connect(cb, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged), [this, cb](int i)
        {
            swctx.getOptions().options_create.create_template = cb->itemData(i).toString().toStdString();
        });
        for (auto &[n,t] : getProjectTemplates().templates)
            cb->addItem(t.desc.c_str(), t.name.c_str());
        cb->setEditable(false);
        l->addWidget(cb);

        l->addWidget(new QLabel("Dependencies"));
        auto cpm = remote_db ? new PackagesModel(*remote_db, true) : nullptr;
        auto le2 = new PackagesLineEdit(cpm);
        l->addWidget(le2);
        auto add = new QPushButton("Add Dependency");
        l->addWidget(add);

        auto wl = new QVBoxLayout;
        l->addLayout(wl);
        auto ve = new StdVectorEdit<String>(swctx.getOptions().options_create.dependencies, wl);

        connect(add, &QPushButton::clicked, [this, cpm, le = le2, ve]()
        {
            if (le->text().isEmpty())
                return;
            if (cpm && cpm->data(cpm->index(0, 0)) != le->text())
                return;

            if (std::find(
                swctx.getOptions().options_create.dependencies.begin(),
                swctx.getOptions().options_create.dependencies.end(), le->text().toStdString()) != swctx.getOptions().options_create.dependencies.end())
                return;

            ve->appendRow() = le->text().toStdString();
            ve->updateWidgets();
        });

        l->addStretch(1);

        cl_option_add_widget("Overwrite existing files (THIS WILL OVERWRITE YOUR CHANGES)",
            l, swctx.getOptions().options_create.create_overwrite_files, swctx.getOptions().getClOptions().subcommand_create_create_overwrite_files, true);

        auto create = new QPushButton("Create");
        connect(create, &QPushButton::clicked, [this]()
        {
            swctx.command_create();
        });
        l->addWidget(create);
    }

    //
    auto add_packages_tab = [this, t](const String &name, auto &db)
    {
        auto v = new QTableView;

        auto m = new PackagesModel(db, true);
        m->single_column_mode = false;
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

    add_packages_tab("Installed Packages", swctx.getContext().getLocalStorage().getPackagesDatabase());
    for (auto rs : swctx.getContext().getRemoteStorages())
    {
        if (auto s1 = dynamic_cast<sw::StorageWithPackagesDatabase *>(rs))
            add_packages_tab("Remote Packages: " + s1->getName(), s1->getPackagesDatabase());
    }

    //
    auto add_text_tab = [t](const String &name, const String &text)
    {
        auto te = new QTextEdit();
        te->setPlainText(text.c_str());
        te->setReadOnly(true);
        t->addTab(te, name.c_str());
    };

    add_text_tab("List of Predefined Targets", swctx.listPredefinedTargets());
    add_text_tab("List of Programs", swctx.listPrograms());

    // raw subcommands
    {
        auto gb = new QGroupBox("Commands");
        QVBoxLayout *gbl = new QVBoxLayout;
        gb->setLayout(gbl);

        auto idx = t->addTab(gb, "Raw Commands");

        QPushButton *b;
#define SUBCOMMAND(x)                            \
    b = new QPushButton(#x);                     \
    connect(b, &QPushButton::clicked, [this]() { \
        swctx.command_##x();                     \
    });                                          \
    gbl->addWidget(b);
#include <sw/client/common/commands.inl>
#undef SUBCOMMAND
    }

    // settings
    {
        auto sa = new QScrollArea;
        //sa->setBackgroundRole(QPalette::Dark);
        sa->setWidgetResizable(true);
        sa->setVerticalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAsNeeded);

        auto idx = t->addTab(sa, "Settings");
        connect(t, &TabWidget::currentChanged, [idx, this, sa](int i)
        {
            if (i != idx)
                return;

            auto setLayout = new QVBoxLayout;
            createOptionWidgets(setLayout, swctx.getOptions());
            //setLayout->addStretch(1);

            auto set = new QWidget;
            set->setLayout(setLayout);
            sa->setWidget(set);
        });
    }

    //
    mainLayout->addWidget(t);

    auto centralWidget = new QWidget;
    centralWidget->setLayout(mainLayout);
    setCentralWidget(centralWidget);
}

void MainWindow::setupGeneral(QWidget *parent)
{
    auto ctrlLayout = new QHBoxLayout;
    parent->setLayout(ctrlLayout);

    auto left = new QVBoxLayout;
    auto middle = new QVBoxLayout;

    ctrlLayout->addLayout(left);
    ctrlLayout->addLayout(middle);

    // left
    {
        auto gb = new QGroupBox("Inputs");
        gb->setMinimumWidth(350);
        left->addWidget(gb, 1);
        QVBoxLayout *gbl = new QVBoxLayout;
        gb->setLayout(gbl);
        auto afile = new QPushButton("Add File");
        auto adir = new QPushButton("Add Directory");

        auto cpm = remote_db ? new PackagesModel(*remote_db, true) : nullptr;
        auto pkgcble = new PackagesLineEdit(cpm);
        auto apkg = new QPushButton("Add Package");
        gbl->addWidget(afile);
        gbl->addWidget(adir);
        gbl->addWidget(pkgcble);
        gbl->addWidget(apkg);

        auto wl = new QVBoxLayout;
        gbl->addLayout(wl);
        auto ve = new StdVectorEdit<String>(swctx.getInputs(), wl);
        auto add_input = [this, ve](const QString &s)
        {
            if (std::find(swctx.getInputs().begin(), swctx.getInputs().end(), s.toStdString()) != swctx.getInputs().end())
                return;
            ve->appendRow() = s.toStdString();
            ve->updateWidgets();
        };

        connect(apkg, &QPushButton::clicked, [add_input, pkgcble, cpm]()
        {
            if (pkgcble->text().isEmpty())
                return;
            if (cpm && cpm->data(cpm->index(0, 0)) != pkgcble->text())
                return;
            add_input(pkgcble->text());
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

    // middle
    {
        auto gbcmd = new QGroupBox("Commands");
        middle->addWidget(gbcmd);
        QVBoxLayout *gblcmd = new QVBoxLayout;
        gbcmd->setLayout(gblcmd);

        // build
        {
            auto b = new QPushButton("Build");
            connect(b, &QPushButton::clicked, [this]()
            {
                swctx.command_build();
            });
            gblcmd->addWidget(b);
        }

        // test
        {
            auto b = new QPushButton("Test");
            connect(b, &QPushButton::clicked, [this]()
            {
                swctx.command_test();
            });
            gblcmd->addWidget(b);
        }

        // open
        {
            auto b = new QPushButton("Open");
            connect(b, &QPushButton::clicked, [this]()
            {
                swctx.command_open();
            });
            gblcmd->addWidget(b);
        }

        // generate
        {
            gblcmd->addWidget(new QLabel("Generate"));
            //
            auto cb = new QComboBox();
            connect(cb, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged), [this, cb](int i)
            {
                swctx.getOptions().options_generate.generator = getGenerators()[cb->itemData(i).toInt()].name;
            });
            cb->setEditable(false);
            for (auto &g : getGenerators())
                cb->addItem(g.name.c_str(), (int)g.type);
            cb->model()->sort(0);

#ifdef _WIN32
            int index = cb->findData((int)GeneratorType::VisualStudio);
            if (index != -1)
                cb->setCurrentIndex(index);
#endif
            gblcmd->addWidget(cb);

            // btn
            {
                auto build = new QPushButton("Generate");
                connect(build, &QPushButton::clicked, [this]()
                {
                    swctx.command_generate();
                });
                gblcmd->addWidget(build);
            }
        }

        //
        gblcmd->addStretch(1);
    }

    middle->addStretch(1);
    //ctrlLayout->addStretch(1);
}

void MainWindow::setupConfiguration(QWidget *parent)
{
    auto cfgLayout = new QHBoxLayout;
    parent->setLayout(cfgLayout);

    auto middle = new QVBoxLayout;
    auto right = new QVBoxLayout;

    cfgLayout->addLayout(middle);
    cfgLayout->addLayout(right);

    auto add_check_box_with_text = [this](auto *parent, const QString &name, const String &val, auto &var)
    {
        auto cb = new QCheckBox(name);
        connect(cb, &QCheckBox::stateChanged, [&var, val](int state)
        {
            if (state)
                var.push_back(val);
            else
                var.erase(std::remove(var.begin(), var.end(), val), var.end());
        });
        parent->addWidget(cb);
        return cb;
    };

    auto add_check_box_bool = [this](auto *parent, const QString &name, auto &var)
    {
        auto cb = new QCheckBox(name);
        connect(cb, &QCheckBox::stateChanged, [&var](int state)
        {
            var = state;
        });
        parent->addWidget(cb);
        return cb;
    };

    // configuration
    {
        auto gb = new QGroupBox("Configuration");
        middle->addWidget(gb);
        QVBoxLayout *gbl = new QVBoxLayout;
        add_check_box_with_text(gbl, "Debug", "debug", swctx.getOptions().configuration);
        add_check_box_with_text(gbl, "Minimal Size Release", "minimalsizerelease", swctx.getOptions().configuration);
        add_check_box_with_text(gbl, "Release With Debug Information", "releasewithdebuginformation", swctx.getOptions().configuration);
        auto cb = add_check_box_with_text(gbl, "Release", "release", swctx.getOptions().configuration);
        cb->setChecked(true);
        gbl->addWidget(cb);
        gb->setLayout(gbl);
    }

    // shared/static
    {
        auto gb = new QGroupBox("Linking");
        middle->addWidget(gb);
        QVBoxLayout *gbl = new QVBoxLayout;
        auto cb = add_check_box_bool(gbl, "Dynamic (.dll/.so/.dylib)", swctx.getOptions().shared_build);
        cb->setChecked(true);
        add_check_box_bool(gbl, "Static (.lib/.a)", swctx.getOptions().static_build);
        gb->setLayout(gbl);
    }

    // mt/md
    {
        auto gb = new QGroupBox("Runtime (Windows only)");
        middle->addWidget(gb);
        QVBoxLayout *gbl = new QVBoxLayout;
        auto cb = add_check_box_bool(gbl, "Dynamic (MD/MDd)", swctx.getOptions().win_md);
        cb->setChecked(true);
        add_check_box_bool(gbl, "Static (MT/MTd)", swctx.getOptions().win_mt);
        gb->setLayout(gbl);
    }

    // arch
    {
        auto gb = new QGroupBox("Architecture");
        middle->addWidget(gb);
        QVBoxLayout *gbl = new QVBoxLayout;
        // basic list
        add_check_box_with_text(gbl, "x86", "x86", swctx.getOptions().platform);
        auto cb = add_check_box_with_text(gbl, "x64", "x64", swctx.getOptions().platform);
        cb->setChecked(true);
        add_check_box_with_text(gbl, "arm", "arm", swctx.getOptions().platform);
        add_check_box_with_text(gbl, "aarch64", "aarch64", swctx.getOptions().platform);
        gbl->addStretch(1);
        gb->setLayout(gbl);
    }

    // compilers
    {
        auto gb = new QGroupBox("Compiler");
        right->addWidget(gb);
        QVBoxLayout *gbl = new QVBoxLayout;
        auto cls = swctx.listCompilers();
        bool set = false;
        for (auto &cl : cls)
        {
            auto gb = new QGroupBox(cl.desc.c_str());
            gbl->addWidget(gb);
            QVBoxLayout *gbl = new QVBoxLayout;
            gb->setLayout(gbl);
            for (auto &[pkg, _] : cl.releases)
                add_check_box_with_text(gbl, pkg.getVersion().toString().c_str(), pkg.toString(), swctx.getOptions().compiler);
            if (!set && !cl.releases.empty())
            {
                ((QCheckBox*)gb->children().back())->setChecked(true);
                set = true;
            }
            for (auto &[pkg,_] : cl.prereleases)
                add_check_box_with_text(gbl, pkg.getVersion().toString().c_str(), pkg.toString(), swctx.getOptions().compiler);
            gbl->addStretch(1);
        }
        gbl->addStretch(1);
        gb->setLayout(gbl);
    }

    auto rb = new QPushButton("Reset");
    connect(rb, &QPushButton::clicked, []()
    {
        //setupConfiguration(cfg);
    });
    middle->addWidget(rb);

    middle->addStretch(1);
    cfgLayout->addStretch(1);
}

void MainWindow::createMenus()
{
    auto aboutAction = new QAction("About");
    connect(aboutAction, &QAction::triggered, [this]
    {
        QMessageBox::information(this, windowTitle(),
            tr("Author: Egor Pugin") + ", 2020-2022\n" +
            ::sw::getProgramName().c_str() + " version " + PACKAGE_VERSION + "\n" +
            primitives::git_rev::getGitRevision().c_str() + "\n" +
            primitives::git_rev::getBuildTime().c_str()
        );
    });

    auto docAction = new QAction("Documentation");
    connect(docAction, &QAction::triggered, [this]
    {
        swctx.command_doc();
    });

    auto updateAction = new QAction("Check for Updates...");
    connect(updateAction, &QAction::triggered, [this]
    {
        self_upgrade(SHORT_PROGRAM_NAME);
        close();
    });

    auto settingsAction = new QAction("Settings");
    connect(settingsAction, &QAction::triggered, [this]
    {
        auto sw = new SettingsWindow(swctx, this);
        sw->setWindowTitle("Settings");
        sw->setWindowModality(Qt::WindowModal);
        sw->show();
    });

    auto exitAction = new QAction("Exit");
    connect(exitAction, &QAction::triggered, [this]
    {
        close();
    });

    auto fileMenu = new QMenu("File");
    fileMenu->addAction(settingsAction);
    fileMenu->addSeparator();
    fileMenu->addAction(exitAction);

    auto helpMenu = new QMenu("Help");
    helpMenu->addAction(docAction);
    helpMenu->addSeparator();
    helpMenu->addAction(updateAction);
    helpMenu->addSeparator();
    helpMenu->addAction(aboutAction);

    auto mainMenu = new QMenuBar;
    mainMenu->addMenu(fileMenu);
    mainMenu->addMenu(helpMenu);

    setMenuBar(mainMenu);
}
