// Copyright (c) 2018 yshurik
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "blockchainpage.h"
#include "ui_blockchainpage.h"
#include "ui_fractionsdialog.h"
#include "txdetailswidget.h"

#include "main.h"
#include "base58.h"
#include "txdb.h"
#include "peg.h"
#include "guiutil.h"
#include "blockchainmodel.h"
#include "itemdelegates.h"
#include "metatypes.h"
#include "qwt/qwt_plot.h"
#include "qwt/qwt_plot_curve.h"
#include "qwt/qwt_plot_barchart.h"

#include <QTime>
#include <QMenu>
#include <QDebug>
#include <QPainter>
#include <QKeyEvent>
#include <QClipboard>
#include <QApplication>

#include <string>
#include <vector>

#include "json/json_spirit_utils.h"
#include "json/json_spirit_writer_template.h"
extern json_spirit::Object blockToJSON(const CBlock& block,
                                       const CBlockIndex* blockindex,
                                       const MapFractions &, 
                                       bool fPrintTransactionDetail);
extern void TxToJSON(const CTransaction& tx,
                     const uint256 hashBlock, 
                     const MapFractions&,
                     int nSupply,
                     json_spirit::Object& entry);

BlockchainPage::BlockchainPage(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::BlockchainPage)
{
    ui->setupUi(this);
    GUIUtil::SetBitBayFonts(this);

    txDetails = new TxDetailsWidget(this);
    txDetails->layout()->setMargin(0);
    auto txDetailsLayout = new QVBoxLayout;
    txDetailsLayout->setMargin(0);
    txDetailsLayout->addWidget(txDetails);
    ui->txDetails->setLayout(txDetailsLayout);

    model = new BlockchainModel(this);
    ui->blockchainView->setModel(model);

    connect(model, SIGNAL(rowsAboutToBeInserted(const QModelIndex &,int,int)),
            this, SLOT(updateCurrentBlockIndex()));
    connect(model, SIGNAL(rowsInserted(const QModelIndex &,int,int)),
            this, SLOT(scrollToCurrentBlockIndex()));

    connect(ui->buttonChain, SIGNAL(clicked()), this, SLOT(showChainPage()));
    connect(ui->buttonBlock, SIGNAL(clicked()), this, SLOT(showBlockPage()));
    connect(ui->buttonTx, SIGNAL(clicked()), this, SLOT(showTxPage()));

    ui->blockchainView->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->blockValues->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->blockchainView->installEventFilter(new BlockchainPageChainEvents(ui->blockchainView, this));
    ui->blockValues->installEventFilter(new BlockchainPageBlockEvents(ui->blockValues, this));

    connect(ui->blockchainView, SIGNAL(customContextMenuRequested(const QPoint &)),
            this, SLOT(openChainMenu(const QPoint &)));
    connect(ui->blockValues, SIGNAL(customContextMenuRequested(const QPoint &)),
            this, SLOT(openBlockMenu(const QPoint &)));

    connect(ui->blockchainView, SIGNAL(activated(const QModelIndex &)),
            this, SLOT(openBlock(const QModelIndex &)));
    connect(ui->blockValues, SIGNAL(itemDoubleClicked(QTreeWidgetItem*,int)),
            this, SLOT(openTx(QTreeWidgetItem*,int)));
    connect(ui->blockValues, SIGNAL(itemDoubleClicked(QTreeWidgetItem*,int)),
            this, SLOT(openBlock(QTreeWidgetItem*,int)));

    QFont font = GUIUtil::bitcoinAddressFont();
    qreal pt = font.pointSizeF()*0.8;
    if (pt != .0) {
        font.setPointSizeF(pt);
    } else {
        int px = font.pixelSize()*8/10;
        font.setPixelSize(px);
    }

    QString hstyle = R"(
        QHeaderView::section {
            background-color: rgb(204,203,227);
            color: rgb(64,64,64);
            padding-left: 4px;
            border: 0px solid #6c6c6c;
            border-right: 1px solid #6c6c6c;
            border-bottom: 1px solid #6c6c6c;
            min-height: 16px;
            text-align: left;
        }
    )";
    ui->blockValues->setStyleSheet(hstyle);
    ui->blockchainView->setStyleSheet(hstyle);

    ui->blockValues->setFont(font);
    ui->blockchainView->setFont(font);

    ui->blockValues->header()->setFont(font);
    ui->blockchainView->header()->setFont(font);

    connect(ui->lineJumpToBlock, SIGNAL(returnPressed()),
            this, SLOT(jumpToBlock()));
    connect(ui->lineFindBlock, SIGNAL(returnPressed()),
            this, SLOT(openBlockFromInput()));
    connect(ui->lineTx, SIGNAL(returnPressed()),
            this, SLOT(openTxFromInput()));
}

BlockchainPage::~BlockchainPage()
{
    delete ui;
}

BlockchainModel * BlockchainPage::blockchainModel() const
{
    return model;
}

void BlockchainPage::showChainPage()
{
    ui->tabs->setCurrentWidget(ui->pageChain);
}

void BlockchainPage::showBlockPage()
{
    ui->tabs->setCurrentWidget(ui->pageBlock);
}

void BlockchainPage::showTxPage()
{
    ui->tabs->setCurrentWidget(ui->pageTx);
}

void BlockchainPage::jumpToBlock()
{
    bool ok = false;
    int blockNum = ui->lineJumpToBlock->text().toInt(&ok);
    if (!ok) return;

    int n = ui->blockchainView->model()->rowCount();
    int r = n-blockNum;
    if (r<0 || r>=n) return;
    auto mi = ui->blockchainView->model()->index(r, 0);
    ui->blockchainView->setCurrentIndex(mi);
    ui->blockchainView->selectionModel()->select(mi, QItemSelectionModel::Current);
    ui->blockchainView->scrollTo(mi, QAbstractItemView::PositionAtCenter);
    ui->blockchainView->setFocus();
    ui->lineJumpToBlock->clear();
}

void BlockchainPage::openBlockFromInput()
{
    bool ok = false;
    int blockNum = ui->lineFindBlock->text().toInt(&ok);
    if (ok) {
        int n = ui->blockchainView->model()->rowCount();
        int r = n-blockNum;
        if (r<0 || r>=n) return;
        auto mi = ui->blockchainView->model()->index(r, 0);
        openBlock(mi);
        return;
    }
    // consider it as hash
    uint256 hash(ui->lineFindBlock->text().toStdString());
    openBlock(hash);
}

void BlockchainPage::updateCurrentBlockIndex()
{
    currentBlockIndex = ui->blockchainView->currentIndex();
}

void BlockchainPage::scrollToCurrentBlockIndex()
{
    ui->blockchainView->scrollTo(currentBlockIndex, QAbstractItemView::PositionAtCenter);
}

void BlockchainPage::openChainMenu(const QPoint & pos)
{
    QModelIndex mi = ui->blockchainView->indexAt(pos);
    if (!mi.isValid()) return;

    QMenu m;
    auto a = m.addAction(tr("Open Block"));
    connect(a, &QAction::triggered, [&] { openBlock(mi); });
    m.addSeparator();
    a = m.addAction(tr("Copy Block Hash"));
    connect(a, &QAction::triggered, [&] {
        QApplication::clipboard()->setText(
            mi.data(BlockchainModel::HashStringRole).toString()
        );
    });
    a = m.addAction(tr("Copy Block Height"));
    connect(a, &QAction::triggered, [&] {
        QApplication::clipboard()->setText(
            mi.data(BlockchainModel::HeightRole).toString()
        );
    });
    a = m.addAction(tr("Copy Block Info"));
    connect(a, &QAction::triggered, [&] {
        auto shash = mi.data(BlockchainModel::HashStringRole).toString();
        uint256 hash(shash.toStdString());
        if (!mapBlockIndex.count(hash))
            return;

        CBlock block;
        auto pblockindex = mapBlockIndex[hash];
        block.ReadFromDisk(pblockindex, true);

        // todo (extended)
        MapFractions mapFractions;
        {
            LOCK(cs_main);
            CPegDB pegdb("r");
            for (const CTransaction & tx : block.vtx) {
                for(size_t i=0; i<tx.vout.size(); i++) {
                    auto fkey = uint320(tx.GetHash(), i);
                    CFractions fractions(0, CFractions::VALUE);
                    pegdb.Read(fkey, fractions);
                    if (fractions.Total() == tx.vout[i].nValue) {
                        mapFractions[fkey] = fractions;
                    }
                }
            }
        }
        
        json_spirit::Value result = blockToJSON(block, pblockindex, mapFractions, false);
        string str = json_spirit::write_string(result, true);

        QApplication::clipboard()->setText(
            QString::fromStdString(str)
        );
    });
    m.exec(ui->blockchainView->viewport()->mapToGlobal(pos));
}

bool BlockchainPageChainEvents::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::KeyRelease) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->matches(QKeySequence::Copy)) {
            QModelIndex mi = treeWidget->currentIndex();
            if (!mi.isValid()) return true;
            auto shash = mi.data(BlockchainModel::HashStringRole).toString();
            uint256 hash(shash.toStdString());
            if (!mapBlockIndex.count(hash))
                return true;

            CBlock block;
            auto pblockindex = mapBlockIndex[hash];
            block.ReadFromDisk(pblockindex, true);

            MapFractions mapFractions;
            {
                LOCK(cs_main);
                CPegDB pegdb("r");
                for (const CTransaction & tx : block.vtx) {
                    for(size_t i=0; i<tx.vout.size(); i++) {
                        auto fkey = uint320(tx.GetHash(), i);
                        CFractions fractions(0, CFractions::VALUE);
                        pegdb.Read(fkey, fractions);
                        if (fractions.Total() == tx.vout[i].nValue) {
                            mapFractions[fkey] = fractions;
                        }
                    }
                }
            }
            
            json_spirit::Value result = blockToJSON(block, pblockindex, mapFractions, false);
            string str = json_spirit::write_string(result, true);

            QApplication::clipboard()->setText(
                QString::fromStdString(str)
            );
            return true;
        }
    }
    return QObject::eventFilter(obj, event);
}

void BlockchainPage::openBlock(const QModelIndex & mi)
{
    if (!mi.isValid())
        return;
    openBlock(mi.data(BlockchainModel::HashRole).value<uint256>());
}

void BlockchainPage::openBlock(QTreeWidgetItem* item, int)
{
    if (item->text(0) == "Next" || item->text(0) == "Previous") {
        uint256 bhash(item->text(1).toStdString());
        openBlock(bhash);
    }
}

void BlockchainPage::openBlock(uint256 hash)
{
    currentBlock = hash;
    QString bhash = QString::fromStdString(currentBlock.ToString());

    LOCK(cs_main);
    if (mapBlockIndex.find(currentBlock) == mapBlockIndex.end())
        return;
    CBlockIndex* pblockindex = mapBlockIndex[currentBlock];
    if (!pblockindex)
        return;
    showBlockPage();
    if (ui->lineFindBlock->text() != bhash && ui->lineFindBlock->text().toInt() != pblockindex->nHeight)
        ui->lineFindBlock->clear();
    ui->blockValues->clear();
    auto topItem = new QTreeWidgetItem(QStringList({"Height",QString::number(pblockindex->nHeight)}));
    QVariant vhash;
    vhash.setValue(hash);
    topItem->setData(0, BlockchainModel::HashRole, vhash);
    ui->blockValues->addTopLevelItem(topItem);
    ui->blockValues->addTopLevelItem(new QTreeWidgetItem(QStringList({"Datetime",QString::fromStdString(DateTimeStrFormat(pblockindex->GetBlockTime()))})));
    ui->blockValues->addTopLevelItem(new QTreeWidgetItem(QStringList({"Hash",bhash})));
    QString pbhash;
    QString nbhash;
    if (pblockindex->pprev) {
        pbhash = QString::fromStdString(pblockindex->pprev->GetBlockHash().ToString());
    }
    if (pblockindex->pnext) {
        nbhash = QString::fromStdString(pblockindex->pnext->GetBlockHash().ToString());
    }
    ui->blockValues->addTopLevelItem(new QTreeWidgetItem(QStringList({"Next",nbhash})));
    ui->blockValues->addTopLevelItem(new QTreeWidgetItem(QStringList({"Previous",pbhash})));
    ui->blockValues->addTopLevelItem(new QTreeWidgetItem(QStringList({"ChainTrust",QString::fromStdString(pblockindex->nChainTrust.ToString())})));

    CBlock block;
    block.ReadFromDisk(pblockindex, true);

    int idx = 0;
    for(const CTransaction & tx : block.vtx) {
        QString stx = "tx"+QString::number(idx);
        QString thash = QString::fromStdString(tx.GetHash().ToString());
        ui->blockValues->addTopLevelItem(new QTreeWidgetItem(QStringList({stx,thash})));
        idx++;
    }
}

void BlockchainPage::openBlockMenu(const QPoint & pos)
{
    QModelIndex mi = ui->blockValues->indexAt(pos);
    if (!mi.isValid()) return;
    auto model = mi.model();
    if (!model) return;

    QMenu m;

    auto a = m.addAction(tr("Copy Value"));
    connect(a, &QAction::triggered, [&] {
        QModelIndex mi2 = model->index(mi.row(), 1 /*value column*/);
        QApplication::clipboard()->setText(
            mi2.data(Qt::DisplayRole).toString()
        );
    });
    a = m.addAction(tr("Copy All Rows"));
    connect(a, &QAction::triggered, [&] {
        QString text;
        for(int r=0; r<model->rowCount(); r++) {
            for(int c=0; c<model->columnCount(); c++) {
                if (c>0) text += "\t";
                QModelIndex mi2 = model->index(r, c);
                text += mi2.data(Qt::DisplayRole).toString();
            }
            text += "\n";
        }
        QApplication::clipboard()->setText(text);
    });
    a = m.addAction(tr("Copy Block Info (json)"));
    connect(a, &QAction::triggered, [&] {
        QModelIndex mi2 = model->index(0, 0); // topItem
        auto hash = mi2.data(BlockchainModel::HashRole).value<uint256>();
        if (!mapBlockIndex.count(hash))
            return;

        CBlock block;
        auto pblockindex = mapBlockIndex[hash];
        block.ReadFromDisk(pblockindex, true);

        MapFractions mapFractions;
        {
            LOCK(cs_main);
            CPegDB pegdb("r");
            for (const CTransaction & tx : block.vtx) {
                for(size_t i=0; i<tx.vout.size(); i++) {
                    auto fkey = uint320(tx.GetHash(), i);
                    CFractions fractions(0, CFractions::VALUE);
                    pegdb.Read(fkey, fractions);
                    if (fractions.Total() == tx.vout[i].nValue) {
                        mapFractions[fkey] = fractions;
                    }
                }
            }
        }
        
        json_spirit::Value result = blockToJSON(block, pblockindex, mapFractions, false);
        string str = json_spirit::write_string(result, true);

        QApplication::clipboard()->setText(
            QString::fromStdString(str)
        );
    });
    m.exec(ui->blockValues->viewport()->mapToGlobal(pos));
}

bool BlockchainPageBlockEvents::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::KeyRelease) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->matches(QKeySequence::Copy)) {
            QModelIndex mi = treeWidget->currentIndex();
            if (!mi.isValid()) return true;
            auto model = mi.model();
            if (!model) return true;
            QModelIndex mi2 = model->index(mi.row(), 1 /*value column*/);
            QApplication::clipboard()->setText(
                mi2.data(Qt::DisplayRole).toString()
            );
            return true;
        }
    }
    return QObject::eventFilter(obj, event);
}

void BlockchainPage::openTxFromInput()
{
    // as height-index
    if (ui->lineTx->text().contains("-")) {
        auto args = ui->lineTx->text().split("-");
        bool ok = false;
        int blockNum = args.front().toInt(&ok);
        if (ok) {
            uint txidx = args.back().toUInt();
            int n = ui->blockchainView->model()->rowCount();
            int r = n-blockNum;
            if (r<0 || r>=n) return;
            auto mi = ui->blockchainView->model()->index(r, 0);
            auto bhash = mi.data(BlockchainModel::HashRole).value<uint256>();
            showTxPage();
            txDetails->openTx(bhash, txidx);
        }
        return;
    }
    // consider it as hash
    uint nTxNum = 0;
    uint256 blockhash;
    uint256 txhash(ui->lineTx->text().toStdString());
    {
        LOCK(cs_main);
        CTxDB txdb("r");
        CTxIndex txindex;
        txdb.ReadTxIndex(txhash, txindex);
        txindex.GetHeightInMainChain(&nTxNum, txhash, &blockhash);
    }
    showTxPage();
    txDetails->openTx(blockhash, nTxNum);
}

void BlockchainPage::openTx(QTreeWidgetItem * item, int column)
{
    Q_UNUSED(column);
    if (item->text(0).startsWith("tx")) { // open from block page
        bool tx_idx_ok = false;
        uint tx_idx = item->text(0).mid(2).toUInt(&tx_idx_ok);
        if (!tx_idx_ok)
            return;

        showTxPage();
        txDetails->openTx(currentBlock, tx_idx);
    }
}

bool BlockchainPageTxEvents::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::KeyRelease) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->matches(QKeySequence::Copy)) {
            QModelIndex mi = treeWidget->currentIndex();
            if (!mi.isValid()) return true;
            auto model = mi.model();
            if (!model) return true;
            QModelIndex mi2 = model->index(mi.row(), 1 /*value column*/);
            QApplication::clipboard()->setText(
                mi2.data(Qt::DisplayRole).toString()
            );
            return true;
        }
    }
    return QObject::eventFilter(obj, event);
}

