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
    void openBlock(const QModelIndex &);
    void openTx(QTreeWidgetItem*,int);
    
private:
    Ui::InfoPage *ui;
    BlockchainModel *model;
    uint256 currentBlock;
};

class FractionsItemDelegate : public QItemDelegate 
{
    Q_OBJECT

public:
    explicit FractionsItemDelegate(QWidget *parent = nullptr);
    ~FractionsItemDelegate();
    
    
    void drawDisplay(QPainter *painter, 
                     const QStyleOptionViewItem &option, 
                     const QRect &rect, 
                     const QString &text) const override;
};

#endif // INFOPAGE_H
