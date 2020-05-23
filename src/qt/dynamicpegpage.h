// Copyright (c) 2020 yshurik
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef DynamicPegPage_H
#define DynamicPegPage_H

#include <QDialog>
#include <QDateTime>

namespace Ui {
    class DynamicPegPage;
}
class WalletModel;

class DynamicPegPage : public QDialog
{
    Q_OBJECT
public:
    explicit DynamicPegPage(QWidget *parent = nullptr);
    ~DynamicPegPage();
    
    void setWalletModel(WalletModel*);
    
public slots:
    void updateTimer();
    
private slots:
    void updatePegVoteType();
    void updateDisplayUnit();
    void setAmounts();
    
private:
    Ui::DynamicPegPage *ui;
    QTimer* pollTimer;
    
    WalletModel* walletModel;
    
    int lastPegVoteType = 0;
    QDateTime lastPegVoteTypeChanged;
};

#endif // DynamicPegPage_H
