// Copyright (c) 2020 yshurik
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "dynamicpegpage.h"
#include "ui_dynamicpegpage.h"

#include "main.h"
#include "init.h"
#include "base58.h"
#include "txdb.h"
#include "peg.h"
#include "guiutil.h"
#include "walletmodel.h"
#include "optionsmodel.h"
#include "bitcoinunits.h"
#include "addressbookpage.h"

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


DynamicPegPage::DynamicPegPage(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DynamicPegPage)
{
    ui->setupUi(this);
    GUIUtil::SetBitBayFonts(this);
    
    setStyleSheet("QRadioButton { background: none; }");

#ifdef Q_OS_MAC
    QFont tfont("Roboto", 15, QFont::Bold);
#else
    QFont tfont("Roboto", 11, QFont::Bold);
#endif
    
//    QString white1 = R"(
//        QWidget {
//            background-color: rgb(255,255,255);
//            padding-left: 10px;
//            padding-right:3px;
//        }
//    )";
//    QString white2 = R"(
//        QWidget {
//            color: rgb(102,102,102);
//            background-color: rgb(255,255,255);
//            padding-left: 10px;
//            padding-right:10px;
//        }
//    )";
    
    pollTimer = new QTimer(this);
    pollTimer->setInterval(30*1000);
    pollTimer->start();
    connect(pollTimer, SIGNAL(timeout()), this, SLOT(updateTimer()));
    
    connect(ui->radioButtonVoteNone, SIGNAL(clicked(bool)), this, SLOT(updatePegVoteType()));
    connect(ui->radioButtonVoteAuto, SIGNAL(clicked(bool)), this, SLOT(updatePegVoteType()));
    connect(ui->radioButtonVoteInflate, SIGNAL(clicked(bool)), this, SLOT(updatePegVoteType()));
    connect(ui->radioButtonVoteDeflate, SIGNAL(clicked(bool)), this, SLOT(updatePegVoteType()));
    connect(ui->radioButtonVoteNoChange, SIGNAL(clicked(bool)), this, SLOT(updatePegVoteType()));
    
    setFocusPolicy(Qt::TabFocus);
}

DynamicPegPage::~DynamicPegPage()
{
    delete ui;
}

void DynamicPegPage::updateTimer()
{
    auto voteType = pwalletMain->GetPegVoteType();
    if (voteType != lastPegVoteType) {
        lastPegVoteType = voteType;
        lastPegVoteTypeChanged = QDateTime::currentDateTime();
    }
    
    if (lastPegVoteType != PEG_VOTE_AUTO && 
            lastPegVoteType != PEG_VOTE_NONE &&
            lastPegVoteTypeChanged.secsTo(QDateTime::currentDateTime()) > ((7*24+12)*60*60)) {
        pwalletMain->SetPegVoteType(PEG_VOTE_AUTO);
        ui->radioButtonVoteAuto->setChecked(true);
    }
}

void DynamicPegPage::updatePegVoteType()
{
    if (ui->radioButtonVoteNone->isChecked()) {
        pwalletMain->SetPegVoteType(PEG_VOTE_NONE);
    } else if (ui->radioButtonVoteAuto->isChecked()) {
        pwalletMain->SetPegVoteType(PEG_VOTE_AUTO);
    } else if (ui->radioButtonVoteInflate->isChecked()) {
        pwalletMain->SetPegVoteType(PEG_VOTE_INFLATE);
    } else if (ui->radioButtonVoteDeflate->isChecked()) {
        pwalletMain->SetPegVoteType(PEG_VOTE_DEFLATE);
    } else if (ui->radioButtonVoteNoChange->isChecked()) {
        pwalletMain->SetPegVoteType(PEG_VOTE_NOCHANGE);
    }
}

void DynamicPegPage::setWalletModel(WalletModel *model)
{
    this->walletModel = model;
    if(model && model->getOptionsModel())
    {
        connect(model->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(updateDisplayUnit()));
        connect(model, SIGNAL(rewardsInfoChanged(qint64,qint64,qint64,qint64, int,int,int,int, int,int,int,int)), 
                this, SLOT(setAmounts()));
    }

    // update the display unit, to not use the default ("BTC")
    updateDisplayUnit();
}

void DynamicPegPage::updateDisplayUnit()
{
    if(walletModel && walletModel->getOptionsModel())
    {
        setAmounts();
    }
}

void DynamicPegPage::setAmounts()
{
    //int unit = walletModel->getOptionsModel()->getDisplayUnit();
    
    double last_peak = pwalletMain->LastPeakPrice();
    PegVoteType last_vote = pwalletMain->LastAutoVoteType();

    ui->radioButtonVoteDeflate->setText(last_vote == PEG_VOTE_DEFLATE 
                                        ? tr("Deflation (auto to %1)").arg(last_peak)
                                        : tr("Deflation"));
    ui->radioButtonVoteInflate->setText(last_vote == PEG_VOTE_INFLATE 
                                        ? tr("Inflation (auto to %1)").arg(last_peak)
                                        : tr("Inflation"));
}
