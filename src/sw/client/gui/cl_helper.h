// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "stdvectoredit.h"

QString option_to_qstring(const String &v)
{
    return v.c_str();
}

QString option_to_qstring(const path &v)
{
    return to_string(to_path_string(normalize_path(v))).c_str();
}

QString option_to_qstring(int v)
{
    return std::to_string(v).c_str();
}

void add_label(const String &name, QBoxLayout *parent, const primitives::cl::Option &o, bool force_name = false)
{
    auto l = new QLabel();
    //l->setWordWrap(true);
    if (force_name)
        l->setText(name.c_str());
    else if (!o.HelpStr.empty())
    {
        l->setText((o.HelpStr.str() + ":").c_str());
        l->setText(l->text() + (" ("s + name + ")").c_str());
        if (!o.ArgStr.empty())
        {
            l->setText(l->text() + " (flag: -");
            l->setText(l->text() + o.ArgStr.c_str());
            l->setText(l->text() + ")");
        }
    }
    else
    {
        l->setText(name.c_str());
        if (!o.ArgStr.empty())
            l->setText(l->text() + (" (flag: -" + o.ArgStr + ")").c_str());
    }
    parent->addWidget(l);
}

template <class T>
void cl_option_add_widget(const String &name, QBoxLayout *parent, std::vector<T> &vector, const primitives::cl::Option &o)
{
    add_label(name, parent, o);

    auto wl = new QVBoxLayout;
    parent->addLayout(wl);

    StdVectorEdit<T> *ve;
    if constexpr (std::is_same_v<T, String>)
        ve = new StdVectorEdit<T>(vector, wl);
    else if constexpr (std::is_same_v<T, path>)
        ve = new StdVectorEdit<T>(vector, wl, [](const path &p) { return to_string(to_path_string(normalize_path(p))); });
    else if constexpr (std::is_same_v<T, int>)
        ve = new StdVectorEdit<T>(vector, wl, [](int i) { return std::to_string(i); }, [](const String &s) { return std::stoi(s); });
    else
        static_assert(false, "Unimplemented type");

    auto b = new QPushButton("Add");
    b->connect(b, &QPushButton::clicked, [ve] { ve->appendRowAndUpdate(); });
    b->connect(b, &QPushButton::destroyed, [ve] { delete ve; });
    wl->addWidget(b);
}

template <class T>
void cl_option_add_widget1(QBoxLayout *parent, T &value, const primitives::cl::Option &o)
{
    auto w = new QLineEdit();
    //if (o.getNumOccurrences())
        w->setText(option_to_qstring(value));
    w->setPlaceholderText(o.ValueStr.str().c_str());
    if constexpr (std::is_floating_point_v<T>)
    {
        w->setValidator(new QDoubleValidator());
        w->connect(w, &QLineEdit::textChanged, [&value](const QString &s)
        {
            value = s.toInt();
        });
    }
    else if constexpr (std::is_integral_v<T>)
    {
        w->setValidator(new QIntValidator());
        w->connect(w, &QLineEdit::textChanged, [&value](const QString &s)
        {
            value = s.toDouble();
        });
    }
    else
    {
        w->connect(w, &QLineEdit::textChanged, [&value](const QString &s)
        {
            value = s.toStdString();
        });
    }
    parent->addWidget(w);
}

template <>
void cl_option_add_widget1(QBoxLayout *parent, bool &value, const primitives::cl::Option &o)
{
    auto w = new QCheckBox();
    w->setChecked(value);
    w->connect(w, &QCheckBox::stateChanged, [&value](int val)
    {
        value = val;
    });
    parent->addWidget(w);
}

template <class T>
void cl_option_add_widget(const String &name, QBoxLayout *parent, T &value, const primitives::cl::Option &o, bool force_name = false)
{
    auto p2 = parent;
    if constexpr (std::is_same_v<T, bool>)
    {
        p2 = new QHBoxLayout;
        parent->addLayout(p2);
        // inverse order
        cl_option_add_widget1(p2, value, o);
        add_label(name, p2, o, force_name);
        p2->addStretch(1);
        return;
    }
    add_label(name, p2, o, force_name);
    cl_option_add_widget1(p2, value, o);
}
