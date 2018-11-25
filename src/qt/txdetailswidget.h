// Copyright (c) 2018 yshurik
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef TXDETAILSWIDGET_H
#define TXDETAILSWIDGET_H

#include <QDialog>
#include <QPixmap>
#include <QItemDelegate>
#include <QStyledItemDelegate>
#include "bignum.h"

namespace Ui {
    class TxDetails;
}

class QTreeView;
class QTreeWidget;
class QModelIndex;
class QTreeWidgetItem;
class BlockchainModel;
class CTransaction;
class CBlockIndex;

class TxDetailsWidget : public QWidget
{
    Q_OBJECT

    enum {
        COL_INP_N = 0,
        COL_INP_TX,
        COL_INP_ADDR,
        COL_INP_VALUE,
        COL_INP_FRACTIONS
    };
    enum {
        COL_OUT_N = 0,
        COL_OUT_TX,
        COL_OUT_ADDR,
        COL_OUT_VALUE,
        COL_OUT_FRACTIONS
    };

public:
    explicit TxDetailsWidget(QWidget *parent = nullptr);
    ~TxDetailsWidget();

public slots:
    void openTx(QTreeWidgetItem*,int);
    void openTx(uint256 blockhash, uint txidx);
    void openTx(CTransaction & tx, 
                CBlockIndex* pblockindex, 
                uint txidx, 
                int nSupply, 
                unsigned int nTime);

private slots:
    void openFractions(QTreeWidgetItem*,int);
    void openFractionsMenu(const QPoint &);
    void openTxMenu(const QPoint &);
    void openInpMenu(const QPoint &);
    void openOutMenu(const QPoint &);

private:
    Ui::TxDetails *ui;
    QPixmap pmChange;
    QPixmap pmNotaryF;
    QPixmap pmNotaryV;
    QPixmap pmNotaryL;
};

class TxDetailsWidgetTxEvents : public QObject
{
    Q_OBJECT
    QTreeWidget* treeWidget;
public:
    TxDetailsWidgetTxEvents(QTreeWidget* w, QObject* parent)
        :QObject(parent), treeWidget(w) {}
    ~TxDetailsWidgetTxEvents() override {}
protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
};

class FractionsDialogEvents : public QObject
{
    Q_OBJECT
    QTreeWidget* treeWidget;
public:
    FractionsDialogEvents(QTreeWidget* w, QObject* parent)
        :QObject(parent), treeWidget(w) {}
    ~FractionsDialogEvents() override {}
protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
};

#endif // TXDETAILSWIDGET_H
