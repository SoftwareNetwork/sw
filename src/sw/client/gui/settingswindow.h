// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2019-2020 Egor Pugin <egor.pugin@gmail.com>

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
