// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2019-2020 Egor Pugin <egor.pugin@gmail.com>

#include "packages_model.h"

#include <qcompleter.h>
#include <sw/manager/package_database.h>

PackagesModel::PackagesModel(sw::PackagesDatabase &s, bool lazy)
    : s(s)
{
    if (!lazy)
        init();
}

void PackagesModel::init()
{
    if (!this->pkgs.empty())
        return;
    setFilter();
}

void PackagesModel::setFilter(const QString &f)
{
    auto filter = f;
    // this will incorrectly replace version
    //filter.replace("-", "_");

    beginResetModel();
    std::set<sw::PackageId> pkgs;
    std::vector<sw::PackagePath> ppaths;
    sw::Version ver;
    bool is_id = false;
    try
    {
        sw::PackageId id(filter.toStdString());
        ppaths = s.getMatchingPackages(id.getPath().toString(), limit);
        ver = id.getVersion();
        is_id = true;
    }
    catch (std::exception &)
    {
        ppaths = s.getMatchingPackages(filter.toStdString(), limit);
    }
    for (auto &ppath : ppaths)
    {
        bool added = false;
        auto vs = s.getVersionsForPackage(ppath);
        for (auto &v : vs)
        {
            if (is_id && v == ver)
            {
                pkgs.emplace(ppath, v);
                added = true;
            }
        }
        if (!added)
        {
            for (auto &v : vs)
                pkgs.emplace(ppath, v);
        }
    }
    this->pkgs.assign(pkgs.begin(), pkgs.end());
    endResetModel();
}

QModelIndex PackagesModel::index(int row, int column, const QModelIndex &parent) const
{
    if (hasIndex(row, column, parent))
        return createIndex(row, column);
    return {};
}

QModelIndex PackagesModel::parent(const QModelIndex &index) const
{
    if (!index.isValid())
        return {};
    if (index.row() == 0)
        return {};
    return index;
}

int PackagesModel::rowCount(const QModelIndex &parent) const
{
    return pkgs.size();
}

int PackagesModel::columnCount(const QModelIndex &parent) const
{
    return single_column_mode ? 1 : 2;
}

QVariant PackagesModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return {};

    if (role == Qt::DisplayRole || role == Qt::EditRole)
    {
        if (single_column_mode)
            return pkgs[index.row()].toString().c_str();
        else
        {
            if (index.column() == 0)
                return pkgs[index.row()].getPath().toString().c_str();
            else
                return pkgs[index.row()].getVersion().toString().c_str();
        }
    }
    return {};
}

PackagesLineEdit::PackagesLineEdit(PackagesModel *cpm, QWidget *parent)
    : QLineEdit(parent)
{
    if (!cpm)
        return;

    auto completer = new QCompleter();
    completer->setModel(cpm);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setFilterMode(Qt::MatchContains);
    completer->setModelSorting(QCompleter::CaseInsensitivelySortedModel);
    completer->setMaxVisibleItems(10);

    connect(this, &PackagesLineEdit::textChanged, [this, completer, cpm](const QString &text)
    {
        if (text.size() < 2)
            setCompleter(nullptr);
        else
        {
            cpm->setFilter(text);
            setCompleter(completer);
        }
    });
}
