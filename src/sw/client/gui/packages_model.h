// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2019-2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include <qabstractitemmodel.h>
#include <qlineedit.h>
#include <sw/manager/package.h>
#include <sw/manager/storage.h>

class PackagesModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    bool single_column_mode = true;
    int limit = 0;

    PackagesModel(sw::PackagesDatabase &, bool lazy = false);

    void init();
    void setFilter(const QString & = {});

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

private:
    sw::PackagesDatabase &s;
    std::vector<sw::PackageId> pkgs;
};

class PackagesLineEdit : public QLineEdit
{
    Q_OBJECT
public:
    PackagesLineEdit(PackagesModel *completion_model, QWidget *parent = nullptr);
};

