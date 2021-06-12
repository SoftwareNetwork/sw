// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include <primitives/string.h>

#include <qboxlayout.h>
#include <qlineedit.h>
#include <qpushbutton.h>
#include <qwidget.h>

template <class T>
struct StdVectorEdit
{
    using TToString = std::function<String(const T &)>;
    using StringToT = std::function<T(const String &)>;

    TToString t_to_string;
    StringToT string_to_t;

    StdVectorEdit(std::vector<T> &data, QBoxLayout *parent,
        TToString t_to_string = [](const String &s) { return s; },
        StringToT string_to_t = [](const String &s) { return s; }
    )
        : v(data), t_to_string(t_to_string), string_to_t(string_to_t)
    {
        lines_layout = new QVBoxLayout;
        parent->addLayout(lines_layout);
        updateWidgets();
    }

    void updateWidgets()
    {
        // create new widgets
        for (int i = lines_layout->children().size(); i < v.size(); i++)
        {
            auto lh = new QHBoxLayout;
            lines_layout->addLayout(lh);

            auto w = new QLineEdit();
            lh->addWidget(w);

            auto b = new QPushButton("Delete");
            b->connect(b, &QPushButton::clicked, [this, lh]()
            {
                auto i = lines_layout->indexOf(lh);
                QLayoutItem *child;
                while ((child = lh->takeAt(0)) != 0)
                {
                    delete child->widget();
                    delete child;
                }
                delete lh;
                deleteRow(i);
            });
            lh->addWidget(b);
        }

        for (int i = 0; i < v.size(); i++)
        {
            auto l = lines_layout->itemAt(i)->layout();
            auto w = (QLineEdit *)l->itemAt(0)->widget();
            w->setText(t_to_string(v[i]).c_str()); // maybe just if constexpr?
            w->disconnect();
            w->connect(w, &QLineEdit::textChanged, [this, &v = v[i]](const QString &text)
            {
                v = string_to_t(text.toStdString());
            });
        }
    }

    T &insertRow(int pos)
    {
        auto val = v.emplace(v.begin() + pos);
        return *val;
    }

    T &appendRow()
    {
        return insertRow(v.size());
    }

    T &appendRowAndUpdate()
    {
        auto &val = appendRow();
        updateWidgets();
        return val;
    }

    void deleteRow(int pos)
    {
        v.erase(v.begin() + pos);
        updateWidgets();
    }

private:
    std::vector<T> &v;
    QVBoxLayout *lines_layout;
};
