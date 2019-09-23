#include "mainwindow.h"

#include <qboxlayout.h>
#include <qheaderview.h>
#include <qtableview.h>

#include <sw/client/common/common.h>
#include <sw/core/sw_context.h>
#include <sw/manager/database.h>
#include <sw/manager/storage.h>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("sw gui");

    setupUi();

    resize(minimumSizeHint());
}

void MainWindow::setupUi()
{
    auto ctx = createSwContext();

    auto pm = new PackagesModel;

    std::set<sw::PackageId> pkgs;
    auto ppaths = ctx->getLocalStorage().getPackagesDatabase().getMatchingPackages();
    for (auto &ppath : ppaths)
    {
        auto vs = ctx->getLocalStorage().getPackagesDatabase().getVersionsForPackage(ppath);
        for (auto &v : vs)
            pkgs.emplace(ppath, v);
    }
    pm->pkgs.assign(pkgs.begin(), pkgs.end());

    auto v = new QTableView;
    v->setModel(pm);
    v->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

    auto mainLayout = new QHBoxLayout;
    mainLayout->addWidget(v);

    auto centralWidget = new QWidget;
    centralWidget->setLayout(mainLayout);
    setCentralWidget(centralWidget);
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
    return 1;
}

QVariant PackagesModel::data(const QModelIndex &index, int role) const
{
    if (role == Qt::DisplayRole)
        return pkgs[index.row()].toString().c_str();
    return {};
}
