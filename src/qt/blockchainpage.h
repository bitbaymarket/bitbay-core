// Copyright (c) 2018 yshurik
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef INFOPAGE_H
#define INFOPAGE_H

#include <QDialog>
#include <QItemDelegate>
#include "bignum.h"

namespace Ui {
    class BlockchainPage;
}

class QModelIndex;
class QTreeWidgetItem;
class BlockchainModel;

class BlockchainPage : public QDialog
{
    Q_OBJECT

public:
    explicit BlockchainPage(QWidget *parent = nullptr);
    ~BlockchainPage();

    BlockchainModel * blockchainModel() const;

private slots:
    void showChainPage();
    void showBlockPage();
    void showTxPage();
    void openBlock(uint256);
    void openBlock(const QModelIndex &);
    void openBlock(QTreeWidgetItem*,int);
    void openTx(QTreeWidgetItem*,int);
    void openTx(uint256 blockhash, uint txidx);
    void openTxFromInput();
    void openFractions(QTreeWidgetItem*,int);
    void jumpToBlock();
    void openBlockFromInput();
    void updateCurrentBlockIndex();
    void scrollToCurrentBlockIndex();
    void openChainMenu(const QPoint &);
    void openBlockMenu(const QPoint &);
    void openTxMenu(const QPoint &);

private:
    Ui::BlockchainPage *ui;
    BlockchainModel *model;
    QPersistentModelIndex currentBlockIndex;
    uint256 currentBlock;
};

class FractionsItemDelegate : public QItemDelegate
{
    Q_OBJECT

public:
    explicit FractionsItemDelegate(QWidget *parent = nullptr);
    ~FractionsItemDelegate() override;


    void paint(QPainter *painter,
               const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
};

#endif // INFOPAGE_H
