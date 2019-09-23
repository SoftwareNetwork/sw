#pragma once

#include <qabstractitemmodel.h>
#include <qmainwindow.h>
#include <sw/manager/package.h>

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    MainWindow(QWidget *parent = 0);

private:
    void setupUi();
};

class PackagesModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    using QAbstractItemModel::QAbstractItemModel;

    std::vector<sw::PackageId> pkgs;

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
};
