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

