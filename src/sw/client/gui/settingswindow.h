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

#pragma once

#include <qmainwindow.h>

#include <memory>
#include <vector>

struct ValueFlusherBase
{
    using OnChange = std::function<void(void)>;

    virtual ~ValueFlusherBase() {}

    virtual void flush() const = 0;
};

template <class T>
struct ValueFlusher : ValueFlusherBase
{
    T &var;
    T temp_var;
    OnChange on_change;

    ValueFlusher(T &in, OnChange f = {}) : var(in), temp_var(in), on_change(f) {}
    void flush() const override { var = temp_var; }
    void set(const T &in)
    {
        if (temp_var == in)
            return;
        temp_var = in;
        if (on_change)
            on_change();
    }
};

struct SwGuiContext;

class SettingsWindow : public QMainWindow
{
    Q_OBJECT

public:
    SettingsWindow(SwGuiContext &swctx, QWidget *parent = 0);

private:
    SwGuiContext &swctx;
    std::vector<std::unique_ptr<ValueFlusherBase>> settings;

    void save();
};
