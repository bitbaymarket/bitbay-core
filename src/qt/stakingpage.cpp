// Copyright (c) 2018 yshurik
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "stakingpage.h"
#include "ui_stakingpage.h"

#include "main.h"
#include "init.h"
#include "base58.h"
#include "txdb.h"
#include "peg.h"
#include "guiutil.h"

#include <QMenu>
#include <QTime>
#include <QTimer>
#include <QDebug>
#include <QPainter>
#include <QKeyEvent>
#include <QClipboard>
#include <QApplication>

#include <string>
#include <vector>


StakingPage::StakingPage(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::StakingPage)
{
    ui->setupUi(this);
    GUIUtil::SetBitBayFonts(this);
    
#ifdef Q_OS_MAC
    QFont hfont("Roboto Black", 20, QFont::Bold);
#else
    QFont hfont("Roboto Black", 15, QFont::Bold);
#endif

    ui->labelTitle->setFont(hfont);
    
    pollTimer = new QTimer(this);
    pollTimer->setInterval(30*1000);
    pollTimer->start();
    connect(pollTimer, SIGNAL(timeout()), this, SLOT(updateTimer()));
}

StakingPage::~StakingPage()
{
    delete ui;
}

extern double GetPoSKernelPS();

void StakingPage::updateTimer()
{
    uint64_t nWeight = 0;
    {
        if (!pwalletMain)
            return;
    
        TRY_LOCK(cs_main, lockMain);
        if (!lockMain)
            return;
    
        TRY_LOCK(pwalletMain->cs_wallet, lockWallet);
        if (!lockWallet)
            return;
    
        nWeight = pwalletMain->GetStakeWeight();
    }
    
    if (nLastCoinStakeSearchInterval && nWeight)
    {
        uint64_t nNetworkWeight = GetPoSKernelPS();
        unsigned nEstimateTime = GetTargetSpacing(nBestHeight) * nNetworkWeight / nWeight;

        QString text;
        if (nEstimateTime < 60)
        {
            text = tr("%n second(s)", "", nEstimateTime);
        }
        else if (nEstimateTime < 60*60)
        {
            text = tr("%n minute(s)", "", nEstimateTime/60);
        }
        else if (nEstimateTime < 24*60*60)
        {
            text = tr("%n hour(s)", "", nEstimateTime/(60*60));
        }
        else
        {
            text = tr("%n day(s)", "", nEstimateTime/(60*60*24));
        }

        nWeight /= COIN;
        nNetworkWeight /= COIN;

        ui->labelText->setText(tr("Staking.<br>Your weight is %1<br>Network weight is %2<br>Expected time to earn reward is %3").arg(nWeight).arg(nNetworkWeight).arg(text));
    }
    else
    {
        if (pwalletMain && pwalletMain->IsLocked())
            ui->labelText->setText(tr("Not staking because wallet is locked"));
        else if (vNodes.empty())
            ui->labelText->setText(tr("Not staking because wallet is offline"));
        else if (IsInitialBlockDownload())
            ui->labelText->setText(tr("Not staking because wallet is syncing"));
        else if (!nWeight)
            ui->labelText->setText(tr("Not staking because you don't have mature coins"));
        else
            ui->labelText->setText(tr("Not staking"));
    }
}
