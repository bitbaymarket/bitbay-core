// Copyright (c) 2018 yshurik
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef STAKINGPAGE_H
#define STAKINGPAGE_H

#include <QDialog>

namespace Ui {
    class StakingPage;
}

class StakingPage : public QDialog
{
    Q_OBJECT
public:
    explicit StakingPage(QWidget *parent = nullptr);
    ~StakingPage();
    
public slots:
    void updateTimer();
    
private slots:
    void updatePegVoteType();
    
private:
    Ui::StakingPage *ui;
    QTimer* pollTimer;
};

#endif // STAKINGPAGE_H
