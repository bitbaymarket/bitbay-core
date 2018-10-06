#ifndef INFOPAGE_H
#define INFOPAGE_H

#include <QDialog>
#include <QItemDelegate>
#include "bignum.h"

namespace Ui {
    class InfoPage;
}

class QModelIndex;
class QTreeWidgetItem;
class BlockchainModel;

class InfoPage : public QDialog
{
    Q_OBJECT

public:
    explicit InfoPage(QWidget *parent = nullptr);
    ~InfoPage();

    BlockchainModel * blockchainModel() const;

private slots:
    void showChainPage();
    void showBlockPage();
    void showTxPage();
    void openBlock(uint256);
    void openBlock(const QModelIndex &);
    void openTx(QTreeWidgetItem*,int);
    void openFractions(QTreeWidgetItem*,int);
    void jumpToBlock();
    void openBlockFromInput();
    void updateCurrentBlockIndex();
    void scrollToCurrentBlockIndex();

private:
    Ui::InfoPage *ui;
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
