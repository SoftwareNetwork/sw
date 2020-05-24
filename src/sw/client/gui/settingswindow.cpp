// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2019-2020 Egor Pugin <egor.pugin@gmail.com>

#include "settingswindow.h"

#include "sw_context.h"

#include <sw/manager/settings.h>

#include <qboxlayout.h>
#include <qfiledialog.h>
#include <qlabel.h>
#include <qlineedit.h>
#include <qpushbutton.h>

std::unique_ptr<ValueFlusherBase> add_path_selector(const String &title, path &var,
    QBoxLayout *parent, QWidget *window, ValueFlusherBase::OnChange on_change)
{
    auto l = new QLabel();
    l->setText(title.c_str());
    parent->addWidget(l);

    auto p2 = new QHBoxLayout;
    parent->addLayout(p2);

    auto vf = std::make_unique<ValueFlusher<path>>(var, on_change);

    auto e = new QLineEdit;
    p2->addWidget(e);
    e->setText(normalize_path(var).c_str());
    e->connect(e, &QLineEdit::textChanged, [&vf = *vf](const QString &t)
    {
        vf.set(t.toStdString());
    });

    auto b = new QPushButton("Change");
    p2->addWidget(b);
    b->connect(b, &QPushButton::clicked, [&var, window, &vf = *vf, e]()
    {
        QFileDialog dialog(window);
        dialog.setFileMode(QFileDialog::Directory);
        if (dialog.exec())
            e->setText(dialog.selectedFiles()[0]);
    });

    return vf;
}

SettingsWindow::SettingsWindow(SwGuiContext &swctx, QWidget *parent)
    : QMainWindow(parent), swctx(swctx)
{
    auto w = new QWidget;
    auto vl = new QVBoxLayout;
    w->setLayout(vl);
    setCentralWidget(w);

    auto ba = new QPushButton("Apply");

    auto flusher = add_path_selector("Storage Directory", sw::Settings::get_user_settings().storage_dir, vl, this,
        [ba] {ba->setEnabled(true); });
    settings.push_back(std::move(flusher));

    //
    auto p2 = new QHBoxLayout;
    vl->addLayout(p2);

    auto bok = new QPushButton("OK");
    p2->addWidget(bok);
    bok->connect(bok, &QPushButton::clicked, [this]()
    {
        save();
        close();
    });

    auto bc = new QPushButton("Cancel");
    p2->addWidget(bc);
    bc->connect(bc, &QPushButton::clicked, [this]()
    {
        close();
    });

    ba->setEnabled(false);
    p2->addWidget(ba);
    ba->connect(ba, &QPushButton::clicked, [this, ba]()
    {
        save();
        ba->setEnabled(false);
    });

    vl->addStretch(1);
}

void SettingsWindow::save()
{
    for (auto &s : settings)
        s->flush();
    sw::Settings::get_user_settings().save(sw::get_config_filename());
    swctx.resetContext(); // apply ctx changes
}
