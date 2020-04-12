/*
 * SW - Build System and Package Manager
 * Copyright (C) 2020 Egor Pugin
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

    StdVectorEdit(std::vector<T> &data, QBoxLayout *parent, TToString t_to_string,
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

    void insertRow(int pos)
    {
        v.emplace(v.begin() + pos);
        updateWidgets();
    }

    void appendRow()
    {
        insertRow(v.size());
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
