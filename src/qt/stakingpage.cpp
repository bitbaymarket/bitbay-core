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
#include "walletmodel.h"
#include "optionsmodel.h"
#include "bitcoinunits.h"

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
    setStyleSheet("QRadioButton { background: none; }");

#ifdef Q_OS_MAC
    QFont tfont("Roboto", 15, QFont::Bold);
#else
    QFont tfont("Roboto", 11, QFont::Bold);
#endif
    ui->labelRewards->setFont(tfont);
    
    QString white1 = R"(
        QWidget {
            background-color: rgb(255,255,255);
            padding-left: 10px;
            padding-right:10px;
        }
    )";
    QString white2 = R"(
        QWidget {
            color: rgb(102,102,102);
            background-color: rgb(255,255,255);
            padding-left: 10px;
            padding-right:10px;
        }
    )";

    ui->labelRewards->setStyleSheet(white1);
    ui->labelSplit->setStyleSheet(white1);
    ui->labelSplitValue->setStyleSheet(white1);
    
    ui->label5Text      ->setStyleSheet(white2);
    ui->label10Text     ->setStyleSheet(white2);
    ui->label20Text     ->setStyleSheet(white2);
    ui->label40Text     ->setStyleSheet(white2);

    ui->label5Count     ->setStyleSheet(white1);
    ui->label10Count    ->setStyleSheet(white1);
    ui->label20Count    ->setStyleSheet(white1);
    ui->label40Count    ->setStyleSheet(white1);
    
    ui->label5Amount    ->setStyleSheet(white1);
    ui->label10Amount   ->setStyleSheet(white1);
    ui->label20Amount   ->setStyleSheet(white1);
    ui->label40Amount   ->setStyleSheet(white1);
    
    pollTimer = new QTimer(this);
    pollTimer->setInterval(30*1000);
    pollTimer->start();
    connect(pollTimer, SIGNAL(timeout()), this, SLOT(updateTimer()));
    
    connect(ui->radioButtonVoteNone, SIGNAL(clicked(bool)), this, SLOT(updatePegVoteType()));
    connect(ui->radioButtonVoteInflate, SIGNAL(clicked(bool)), this, SLOT(updatePegVoteType()));
    connect(ui->radioButtonVoteDeflate, SIGNAL(clicked(bool)), this, SLOT(updatePegVoteType()));
    connect(ui->radioButtonVoteNoChange, SIGNAL(clicked(bool)), this, SLOT(updatePegVoteType()));
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

void StakingPage::updatePegVoteType()
{
    if (ui->radioButtonVoteNone->isChecked()) {
        pwalletMain->SetPegVoteType(PEG_VOTE_NONE);
    } else if (ui->radioButtonVoteInflate->isChecked()) {
        pwalletMain->SetPegVoteType(PEG_VOTE_INFLATE);
    } else if (ui->radioButtonVoteDeflate->isChecked()) {
        pwalletMain->SetPegVoteType(PEG_VOTE_DEFLATE);
    } else if (ui->radioButtonVoteNoChange->isChecked()) {
        pwalletMain->SetPegVoteType(PEG_VOTE_NOCHANGE);
    }
}

void StakingPage::setWalletModel(WalletModel *model)
{
    this->walletModel = model;
    if(model && model->getOptionsModel())
    {
        vector<RewardInfo> vRewardsInfo;
        vRewardsInfo.push_back({PEG_REWARD_5 ,0,0,0});
        vRewardsInfo.push_back({PEG_REWARD_10,0,0,0});
        vRewardsInfo.push_back({PEG_REWARD_20,0,0,0});
        vRewardsInfo.push_back({PEG_REWARD_40,0,0,0});
        model->getRewardInfo(vRewardsInfo);
        
        // Keep up to date with wallet
        setAmounts(vRewardsInfo[PEG_REWARD_5 ].amount,
                   vRewardsInfo[PEG_REWARD_10].amount,
                   vRewardsInfo[PEG_REWARD_20].amount,
                   vRewardsInfo[PEG_REWARD_40].amount,
                   
                   vRewardsInfo[PEG_REWARD_5 ].count,
                   vRewardsInfo[PEG_REWARD_10].count,
                   vRewardsInfo[PEG_REWARD_20].count,
                   vRewardsInfo[PEG_REWARD_40].count,
                   
                   vRewardsInfo[PEG_REWARD_5 ].stake,
                   vRewardsInfo[PEG_REWARD_10].stake,
                   vRewardsInfo[PEG_REWARD_20].stake,
                   vRewardsInfo[PEG_REWARD_40].stake);
        connect(model, SIGNAL(rewardsInfoChanged(qint64,qint64,qint64,qint64, int,int,int,int, int,int,int,int)), 
                this, SLOT(setAmounts(qint64,qint64,qint64,qint64, int,int,int,int, int,int,int,int)));

        connect(model->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(updateDisplayUnit()));
    }

    // update the display unit, to not use the default ("BTC")
    updateDisplayUnit();
}

void StakingPage::updateDisplayUnit()
{
    if(walletModel && walletModel->getOptionsModel())
    {
        vector<RewardInfo> vRewardsInfo;
        vRewardsInfo.push_back({PEG_REWARD_5 ,0,0,0});
        vRewardsInfo.push_back({PEG_REWARD_10,0,0,0});
        vRewardsInfo.push_back({PEG_REWARD_20,0,0,0});
        vRewardsInfo.push_back({PEG_REWARD_40,0,0,0});
        walletModel->getRewardInfo(vRewardsInfo);
        
        setAmounts(vRewardsInfo[PEG_REWARD_5 ].amount,
                   vRewardsInfo[PEG_REWARD_10].amount,
                   vRewardsInfo[PEG_REWARD_20].amount,
                   vRewardsInfo[PEG_REWARD_40].amount,
                   
                   vRewardsInfo[PEG_REWARD_5 ].count,
                   vRewardsInfo[PEG_REWARD_10].count,
                   vRewardsInfo[PEG_REWARD_20].count,
                   vRewardsInfo[PEG_REWARD_40].count,
                   
                   vRewardsInfo[PEG_REWARD_5 ].stake,
                   vRewardsInfo[PEG_REWARD_10].stake,
                   vRewardsInfo[PEG_REWARD_20].stake,
                   vRewardsInfo[PEG_REWARD_40].stake);
    }
}

void StakingPage::setAmounts(qint64 reward5, qint64 reward10, qint64 reward20, qint64 reward40, 
                             int count5, int count10, int count20, int count40,
                             int stake5, int stake10, int stake20, int stake40)
{
    int unit = walletModel->getOptionsModel()->getDisplayUnit();
    
    current5Amount  = reward5;
    current10Amount = reward10;
    current20Amount = reward20;
    current40Amount = reward40;

    current5Count  = count5;
    current10Count = count10;
    current20Count = count20;
    current40Count = count40;

    current5Stake  = stake5;
    current10Stake = stake10;
    current20Stake = stake20;
    current40Stake = stake40;
    
    ui->label5Amount->setText(BitcoinUnits::formatWithUnitForLabel(unit, reward5));
    ui->label5Count->setText(tr("(%1)").arg(count5));
    if (stake5)
        ui->label5Count->setText(tr("(%1/%2)").arg(stake5).arg(count5));
    
    ui->label10Amount->setText(BitcoinUnits::formatWithUnitForLabel(unit, reward10));
    ui->label10Count->setText(tr("(%1)").arg(count10));
    if (stake10)
        ui->label10Count->setText(tr("(%1/%2)").arg(stake10).arg(count10));
    
    ui->label20Amount->setText(BitcoinUnits::formatWithUnitForLabel(unit, reward20));
    ui->label20Count->setText(tr("(%1)").arg(count20));
    if (stake20)
        ui->label20Count->setText(tr("(%1/%2)").arg(stake20).arg(count20));
    
    ui->label40Amount->setText(BitcoinUnits::formatWithUnitForLabel(unit, reward40));
    ui->label40Count->setText(tr("(%1)").arg(count40));
    if (stake40)
        ui->label40Count->setText(tr("(%1/%2)").arg(stake40).arg(count40));
}

