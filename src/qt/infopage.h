#ifndef INFOPAGE_H
#define INFOPAGE_H

#include <QDialog>
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
    explicit InfoPage(QWidget *parent = 0);
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

#endif // INFOPAGE_H
