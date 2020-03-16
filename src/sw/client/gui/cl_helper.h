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

QString option_to_qstring(const String &v)
{
    return v.c_str();
}

QString option_to_qstring(const path &v)
{
    return normalize_path(v).c_str();
}

QString option_to_qstring(int v)
{
    return std::to_string(v).c_str();
}

void add_label(const String &name, QLayout *parent, const primitives::cl::Option &o)
{
    auto l = new QLabel();
    if (!o.HelpStr.empty())
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

void cl_option_add_widget(const String &name, QLayout *parent, bool &value, const primitives::cl::Option &o)
{
    add_label(name, parent, o);
    auto w = new QCheckBox();
    w->setChecked(value);
    w->connect(w, &QCheckBox::stateChanged, [&value](int val)
    {
        value = val;
    });
    parent->addWidget(w);
}

template <class T>
void cl_option_add_widget(const String &name, QLayout *parent, std::vector<T> &value, const primitives::cl::Option &o)
{
    add_label(name, parent, o);
    auto w = new QWidget;
    auto wl = new QVBoxLayout;
    wl->setMargin(0);
    w->setLayout(wl);
    for (auto &v : value)
        cl_option_add_widget1(wl, v, o);
    auto b = new QPushButton("Add");
    b->connect(b, &QPushButton::clicked, [wl, &value, &o]()
    {
        value.push_back({});
        cl_option_add_widget1(wl, value.back(), o);
    });
    parent->addWidget(w);
    parent->addWidget(b);
}

template <class T>
void cl_option_add_widget1(QLayout *parent, T &value, const primitives::cl::Option &o)
{
    auto w = new QLineEdit();
    w->setText(option_to_qstring(value));
    w->setPlaceholderText(o.ValueStr.str().c_str());
    parent->addWidget(w);
}

template <class T>
void cl_option_add_widget(const String &name, QLayout *parent, T &value, const primitives::cl::Option &o)
{
    add_label(name, parent, o);
    cl_option_add_widget1(parent, value, o);
}
