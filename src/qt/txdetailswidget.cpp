// Copyright (c) 2018 yshurik
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "txdetailswidget.h"
#include "ui_txdetails.h"
#include "ui_fractionsdialog.h"

#include "main.h"
#include "base58.h"
#include "txdb.h"
#include "peg.h"
#include "guiutil.h"
#include "blockchainmodel.h"
#include "itemdelegates.h"
#include "metatypes.h"
#include "qwt/qwt_plot.h"
#include "qwt/qwt_plot_item.h"
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

TxDetailsWidget::TxDetailsWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::TxDetails)
{
    ui->setupUi(this);
    GUIUtil::SetBitBayFonts(this);

    ui->txValues->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->txInputs->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->txOutputs->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->txValues->installEventFilter(new TxDetailsWidgetTxEvents(ui->txValues, this));

    connect(ui->txValues, SIGNAL(customContextMenuRequested(const QPoint &)),
            this, SLOT(openTxMenu(const QPoint &)));
    connect(ui->txInputs, SIGNAL(customContextMenuRequested(const QPoint &)),
            this, SLOT(openInpMenu(const QPoint &)));
    connect(ui->txOutputs, SIGNAL(customContextMenuRequested(const QPoint &)),
            this, SLOT(openOutMenu(const QPoint &)));

    connect(ui->txInputs, SIGNAL(itemDoubleClicked(QTreeWidgetItem*,int)),
            this, SLOT(openFractions(QTreeWidgetItem*,int)));
    connect(ui->txOutputs, SIGNAL(itemDoubleClicked(QTreeWidgetItem*,int)),
            this, SLOT(openFractions(QTreeWidgetItem*,int)));
    connect(ui->txInputs, SIGNAL(itemDoubleClicked(QTreeWidgetItem*,int)),
            this, SLOT(openTx(QTreeWidgetItem*,int)));
    connect(ui->txOutputs, SIGNAL(itemDoubleClicked(QTreeWidgetItem*,int)),
            this, SLOT(openTx(QTreeWidgetItem*,int)));

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
    ui->txValues->setStyleSheet(hstyle);
    ui->txInputs->setStyleSheet(hstyle);
    ui->txOutputs->setStyleSheet(hstyle);

    ui->txValues->setFont(font);
    ui->txInputs->setFont(font);
    ui->txOutputs->setFont(font);

    ui->txValues->header()->setFont(font);
    ui->txInputs->header()->setFont(font);
    ui->txOutputs->header()->setFont(font);

    ui->txValues->header()->resizeSection(0 /*property*/, 250);
    ui->txInputs->header()->resizeSection(0 /*n*/, 50);
    ui->txOutputs->header()->resizeSection(0 /*n*/, 50);
    ui->txInputs->header()->resizeSection(1 /*tx*/, 140);
    ui->txOutputs->header()->resizeSection(1 /*tx*/, 140);
    ui->txInputs->header()->resizeSection(2 /*addr*/, 280);
    ui->txOutputs->header()->resizeSection(2 /*addr*/, 280);
    ui->txInputs->header()->resizeSection(3 /*value*/, 180);
    ui->txOutputs->header()->resizeSection(3 /*value*/, 180);

    auto txInpValueDelegate = new LeftSideIconItemDelegate(ui->txInputs);
    auto txOutValueDelegate = new LeftSideIconItemDelegate(ui->txOutputs);
    ui->txInputs->setItemDelegateForColumn(3 /*value*/, txInpValueDelegate);
    ui->txOutputs->setItemDelegateForColumn(3 /*value*/, txOutValueDelegate);

    auto txInpFractionsDelegate = new FractionsItemDelegate(ui->txInputs);
    auto txOutFractionsDelegate = new FractionsItemDelegate(ui->txOutputs);
    ui->txInputs->setItemDelegateForColumn(4 /*frac*/, txInpFractionsDelegate);
    ui->txOutputs->setItemDelegateForColumn(4 /*frac*/, txOutFractionsDelegate);

    pmChange = QPixmap(":/icons/change");
    pmChange = pmChange.scaled(32,32, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    pmNotaryF = QPixmap(":/icons/frost");
    pmNotaryF = pmNotaryF.scaled(32,32, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    pmNotaryV = QPixmap(":/icons/frostl");
    pmNotaryV = pmNotaryV.scaled(32,32, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

TxDetailsWidget::~TxDetailsWidget()
{
    delete ui;
}

void TxDetailsWidget::openTx(QTreeWidgetItem * item, int column)
{
    if ((sender() == ui->txInputs || sender() == ui->txOutputs) && column == 1) {
        uint256 txhash = item->data(1, BlockchainModel::HashRole).value<uint256>();
        CTxDB txdb("r");
        CTxIndex txindex;
        txdb.ReadTxIndex(txhash, txindex);
        uint nTxNum = 0;
        uint256 blockhash;
        txindex.GetHeightInMainChain(&nTxNum, txhash, &blockhash);
        openTx(blockhash, nTxNum);
    }
}

static QString scriptToAddress(const CScript& scriptPubKey,
                               bool& is_notary,
                               bool show_alias =true) {
    is_notary = false;
    int nRequired;
    txnouttype type;
    vector<CTxDestination> addresses;
    if (ExtractDestinations(scriptPubKey, type, addresses, nRequired)) {
        std::string str_addr_all;
        bool none = true;
        for(const CTxDestination& addr : addresses) {
            std::string str_addr = CBitcoinAddress(addr).ToString();
            if (show_alias) {
                if (str_addr == PEG_INFLATE_ADDR) {
                    str_addr = "peginflate";
                }
                else if (str_addr == PEG_DEFLATE_ADDR) {
                    str_addr = "pegdeflate";
                }
                else if (str_addr == PEG_NOCHANGE_ADDR) {
                    str_addr = "pegnochange";
                }
            }
            if (!str_addr_all.empty())
                str_addr_all += "\n";
            str_addr_all += str_addr;
            none = false;
        }
        if (!none)
            return QString::fromStdString(str_addr_all);
    }
    const CScript& script1 = scriptPubKey;

    opcodetype opcode1;
    vector<unsigned char> vch1;
    CScript::const_iterator pc1 = script1.begin();
    if (!script1.GetOp(pc1, opcode1, vch1))
        return QString();

    if (opcode1 == OP_RETURN && script1.size()>1) {
        is_notary = true;
        QString left_bytes;
        unsigned long len_bytes = script1[1];
        if (len_bytes > script1.size()-2)
            len_bytes = script1.size()-2;
        for(unsigned int i=0; i< len_bytes; i++) {
            left_bytes += char(script1[i+2]);
        }
        return left_bytes;
    }

    return QString();
}

static QString displayValue(int64_t nValue) {
    QString sValue = QString::number(nValue);
    if (sValue.length() <8) {
        sValue = sValue.rightJustified(8, QChar(' '));
    }
    sValue.insert(sValue.length()-8, QChar('.'));
    if (sValue.length() > (8+1+3))
        sValue.insert(sValue.length()-8-1-3, QChar(','));
    if (sValue.length() > (8+1+3+1+3))
        sValue.insert(sValue.length()-8-1-3-1-3, QChar(','));
    if (sValue.length() > (8+1+3+1+3+1+3))
        sValue.insert(sValue.length()-8-1-3-1-3-1-3, QChar(','));
    return sValue;
}

static QString displayValueR(int64_t nValue, int len=0) {
    if (len==0) {
        len = 8+1+3+1+3+1+3+5;
    }
    QString sValue = displayValue(nValue);
    sValue = sValue.rightJustified(len, QChar(' '));
    return sValue;
}

static QString txId(CTxDB& txdb, uint256 txhash) {
    QString txid;
    CTxIndex txindex;
    txdb.ReadTxIndex(txhash, txindex);
    uint nTxNum = 0;
    int nHeight = txindex.GetHeightInMainChain(&nTxNum, txhash);
    if (nHeight >0) {
        txid = QString("%1-%2").arg(nHeight).arg(nTxNum);
    }
    return txid;
}

static bool calculateFeesFractions(CBlockIndex* pblockindex,
                                   CFractions& feesFractions,
                                   int64_t& nFeesValue)
{
    CBlock block;
    block.ReadFromDisk(pblockindex, true);
    
    MapPrevTx mapInputs;
    MapFractions mapInputsFractions;
    map<uint256, CTxIndex> mapUnused;
    MapFractions mapFractionsUnused;
    string sPegFailCause;

    for (CTransaction & tx : block.vtx) {

        if (tx.IsCoinBase()) continue;
        if (tx.IsCoinStake()) continue;

        uint256 hash = tx.GetHash();

        CTxDB txdb("r");
        CPegDB pegdb("r");
        if (!txdb.ContainsTx(hash))
            return false;

        vector<int> vOutputsTypes;
        bool fInvalid = false;
        tx.FetchInputs(txdb, pegdb, mapUnused, mapFractionsUnused, false, false, mapInputs, mapInputsFractions, fInvalid);

        int64_t nTxValueIn = tx.GetValueIn(mapInputs);
        int64_t nTxValueOut = tx.GetValueOut();
        nFeesValue += nTxValueIn - nTxValueOut;

        if (!IsPegWhiteListed(tx, mapInputs)) {
            continue;
        }

        bool peg_ok = CalculateStandardFractions(tx,
                                                 pblockindex->nPegSupplyIndex,
                                                 pblockindex->nTime,
                                                 mapInputs, mapInputsFractions,
                                                 mapUnused, mapFractionsUnused,
                                                 feesFractions,
                                                 vOutputsTypes,
                                                 sPegFailCause);

        if (!peg_ok)
            return false;
    }

    return true;
}

void TxDetailsWidget::openTx(uint256 blockhash, uint txidx)
{
    LOCK(cs_main);
    if (mapBlockIndex.find(blockhash) == mapBlockIndex.end())
        return;
    CBlockIndex* pblockindex = mapBlockIndex[blockhash];
    if (!pblockindex)
        return;

    CBlock block;
    block.ReadFromDisk(pblockindex, true);
    if (txidx >= block.vtx.size())
        return;

    CTransaction & tx = block.vtx[txidx];
    openTx(tx, pblockindex, txidx, pblockindex->nPegSupplyIndex, pblockindex->nTime);
}

void TxDetailsWidget::openTx(CTransaction & tx, 
                             CBlockIndex* pblockindex, 
                             uint txidx, 
                             int nSupply, 
                             unsigned int nTime)
{
    LOCK(cs_main);

    uint256 hash = tx.GetHash();

    CTxDB txdb("r");
    CPegDB pegdb("r");

    ui->txValues->clear();
    if (pblockindex) {
        QString thash = QString::fromStdString(hash.ToString());
        QString sheight = QString("%1-%2").arg(pblockindex->nHeight).arg(txidx);
        auto topItem = new QTreeWidgetItem(QStringList({"Height",sheight}));
        QVariant vhash;
        vhash.setValue(hash);
        topItem->setData(0, BlockchainModel::HashRole, vhash);
        ui->txValues->addTopLevelItem(topItem);
        ui->txValues->addTopLevelItem(new QTreeWidgetItem(QStringList({"Datetime",QString::fromStdString(DateTimeStrFormat(pblockindex->GetBlockTime()))})));
        ui->txValues->addTopLevelItem(new QTreeWidgetItem(QStringList({"Hash",thash})));
    }

    QString txtype = tr("Transaction");
    if (tx.IsCoinBase()) txtype = tr("CoinBase");
    if (tx.IsCoinStake()) txtype = tr("CoinStake");
    ui->txValues->addTopLevelItem(new QTreeWidgetItem(QStringList({"Type",txtype})));
    ui->txValues->addTopLevelItem(new QTreeWidgetItem(QStringList({"Peg Supply Index",QString::number(nSupply)})));

    if (tx.IsCoinStake() && !pblockindex) {
        ui->txValues->addTopLevelItem(new QTreeWidgetItem(QStringList({"Error","No blockindex provided"})));
        return;
    }
    
    // logic

    QTime timeFetchInputs = QTime::currentTime();
    MapPrevTx mapInputs;
    MapFractions mapInputsFractions;
    map<uint256, CTxIndex> mapUnused;
    MapFractions mapFractionsUnused;
    CFractions feesFractions(0, CFractions::STD);
    int64_t nFeesValue = 0;
    vector<int> vOutputsTypes;
    string sPegFailCause;
    bool fInvalid = false;
    tx.FetchInputs(txdb, pegdb, mapUnused, mapFractionsUnused, false, false, mapInputs, mapInputsFractions, fInvalid);
    int msecsFetchInputs = timeFetchInputs.msecsTo(QTime::currentTime());

    int64_t nReserveIn = 0;
    int64_t nLiquidityIn = 0;
    for(auto const & inputFractionItem : mapInputsFractions) {
        inputFractionItem.second.LowPart(nSupply, &nReserveIn);
        inputFractionItem.second.HighPart(nSupply, &nLiquidityIn);
    }

    bool peg_whitelisted = IsPegWhiteListed(tx, mapInputs);
    bool peg_ok = false;

    // need to collect all fees fractions from all tx in the block
    if (tx.IsCoinStake() && !calculateFeesFractions(pblockindex, feesFractions, nFeesValue)) {
        ui->txValues->addTopLevelItem(new QTreeWidgetItem(QStringList({"Error","Failed to calculate fees fractions"})));
        return;
    }
    
    QTime timePegChecks = QTime::currentTime();
    if (tx.IsCoinStake()) {
        uint64_t nCoinAge = 0;
        if (!tx.GetCoinAge(txdb, pblockindex->pprev, nCoinAge)) {
            ui->txValues->addTopLevelItem(new QTreeWidgetItem(QStringList({"Error","Cannot get coin age"})));
            return;
        }
        CFractions inpStake(0, CFractions::STD);
        if (tx.vin.size() > 0) {
            const COutPoint & prevout = tx.vin.front().prevout;
            auto fkey = uint320(prevout.hash, prevout.n);
            if (mapInputsFractions.find(fkey) != mapInputsFractions.end()) {
                inpStake = mapInputsFractions[fkey];
            }
        }
        int64_t nDemoSubsidy = 0;
        int64_t nStakeRewardWithoutFees = GetProofOfStakeReward(
                    pblockindex->pprev, nCoinAge, 0 /*fees*/, inpStake, nDemoSubsidy);
        
        peg_ok = CalculateStakingFractions(tx, pblockindex,
                                           mapInputs, mapInputsFractions,
                                           mapUnused, mapFractionsUnused,
                                           feesFractions,
                                           nStakeRewardWithoutFees,
                                           vOutputsTypes,
                                           sPegFailCause);
    }
    else {
        peg_ok = CalculateStandardFractions(tx,
                                            nSupply,
                                            nTime,
                                            mapInputs, mapInputsFractions,
                                            mapUnused, mapFractionsUnused,
                                            feesFractions,
                                            vOutputsTypes,
                                            sPegFailCause);
    }
    int msecsPegChecks = timePegChecks.msecsTo(QTime::currentTime());

    size_t n_vin = tx.vin.size();
    size_t n_vout = tx.vout.size();
    if (tx.IsCoinBase()) n_vin = 0;
    if (tx.IsCoinBase()) n_vout = 0;
    
    int64_t nValueIn = 0;
    for (unsigned int i = 0; i < n_vin; i++) {
        COutPoint prevout = tx.vin[i].prevout;
        if (mapInputs.find(prevout.hash) != mapInputs.end()) {
            CTransaction& txPrev = mapInputs[prevout.hash].second;
            if (prevout.n < txPrev.vout.size()) {
                int64_t nValue = txPrev.vout[prevout.n].nValue;
                nValueIn += nValue;
            }
        }
    }
    int64_t nValueOut = 0;
    for (unsigned int i = 0; i < n_vout; i++) {
        int64_t nValue = tx.vout[i].nValue;
        nValueOut += nValue;
    }
    
    // gui

    ui->txInputs->clear();
    QSet<QString> sAddresses;
    for (unsigned int i = 0; i < n_vin; i++)
    {
        COutPoint prevout = tx.vin[i].prevout;
        QStringList row;
        row << QString::number(i); // idx, 0

        QString prev_thash = QString::fromStdString(prevout.hash.ToString());
        QString prev_txid_hash = prev_thash.left(4)+"..."+prev_thash.right(4);
        QString prev_txid_height = txId(txdb, prevout.hash);
        QString prev_txid = prev_txid_height.isEmpty() ? prev_txid_hash : prev_txid_height;

        row << QString("%1:%2").arg(prev_txid).arg(prevout.n); // tx, 1
        auto prev_input = QString("%1:%2").arg(prev_thash).arg(prevout.n); // tx, 1
        int64_t nValue = 0;

        if (mapInputs.find(prevout.hash) != mapInputs.end()) {
            CTransaction& txPrev = mapInputs[prevout.hash].second;
            if (prevout.n < txPrev.vout.size()) {
                bool is_notary = false;
                auto addr = scriptToAddress(txPrev.vout[prevout.n].scriptPubKey, is_notary);
                if (addr.isEmpty())
                    row << "N/A"; // address, 2
                else {
                    row << addr;
                    sAddresses.insert(addr);
                }

                nValue = txPrev.vout[prevout.n].nValue;
                row << displayValue(nValue);
            }
            else {
                row << "N/A"; // address, 2
                row << "none"; // value, 3
            }
        }
        else {
            row << "N/A"; // address
            row << "none"; // value
        }

        auto input = new QTreeWidgetItem(row);
        {
            QVariant vhash;
            vhash.setValue(prevout.hash);
            input->setData(COL_INP_TX, BlockchainModel::HashRole, vhash);
            input->setData(COL_INP_TX, BlockchainModel::OutNumRole, prevout.n);
        }
        auto fkey = uint320(prevout.hash, prevout.n);
        if (mapInputsFractions.find(fkey) != mapInputsFractions.end()) {
            QVariant vfractions;
            vfractions.setValue(mapInputsFractions[fkey]);
            input->setData(COL_INP_FRACTIONS, BlockchainModel::HashRole, prev_input);
            input->setData(COL_INP_FRACTIONS, BlockchainModel::FractionsRole, vfractions);
            input->setData(COL_INP_FRACTIONS, BlockchainModel::PegSupplyRole, peg_whitelisted
                           ? nSupply
                           : 0);
            if (mapInputsFractions[fkey].nFlags & CFractions::NOTARY_F) {
                input->setData(COL_INP_VALUE, Qt::DecorationPropertyRole, pmNotaryF);
            }
            else if (mapInputsFractions[fkey].nFlags & CFractions::NOTARY_V) {
                input->setData(COL_INP_VALUE, Qt::DecorationPropertyRole, pmNotaryV);
            }
        }
        input->setData(COL_INP_VALUE, Qt::TextAlignmentRole, int(Qt::AlignVCenter | Qt::AlignRight));
        input->setData(COL_INP_VALUE, BlockchainModel::ValueForCopy, qlonglong(nValue));
        ui->txInputs->addTopLevelItem(input);
    }

    if (tx.IsCoinStake()) {
        uint64_t nCoinAge = 0;
        if (!tx.GetCoinAge(txdb, pblockindex->pprev, nCoinAge)) {
            ui->txValues->addTopLevelItem(new QTreeWidgetItem(QStringList({"Error","Cannot get coin age"})));
            return;
        }
        CFractions inpStake(0, CFractions::STD);
        if (tx.vin.size() > 0) {
            const COutPoint & prevout = tx.vin.front().prevout;
            auto fkey = uint320(prevout.hash, prevout.n);
            if (mapInputsFractions.find(fkey) != mapInputsFractions.end()) {
                inpStake = mapInputsFractions[fkey];
            }
        }

        QStringList rowMined;
        rowMined << "Mined"; // idx, 0
        rowMined << "";      // tx, 1
        rowMined << "N/A";   // address, 2
        rowMined << displayValue(nValueOut - nValueIn);

        auto inputMined = new QTreeWidgetItem(rowMined);
        QVariant vfractions;
        vfractions.setValue(CFractions(nValueOut - nValueIn, CFractions::STD));
        inputMined->setData(COL_INP_FRACTIONS, BlockchainModel::FractionsRole, vfractions);
        inputMined->setData(COL_INP_FRACTIONS, BlockchainModel::PegSupplyRole, peg_whitelisted
                            ? nSupply
                            : 0);
        inputMined->setData(COL_INP_VALUE, Qt::TextAlignmentRole, int(Qt::AlignVCenter | Qt::AlignRight));
        inputMined->setData(COL_INP_VALUE, BlockchainModel::ValueForCopy, qlonglong(nValueOut - nValueIn));
        ui->txInputs->addTopLevelItem(inputMined);

        QStringList rowFees;
        rowFees << "Fees";  // idx, 0
        rowFees << "";      // tx, 1
        rowFees << "N/A";   // address, 2
        rowFees << displayValue(nFeesValue);

        auto inputFees = new QTreeWidgetItem(rowFees);
        vfractions.setValue(feesFractions);
        inputFees->setData(COL_INP_FRACTIONS, BlockchainModel::FractionsRole, vfractions);
        inputFees->setData(COL_INP_FRACTIONS, BlockchainModel::PegSupplyRole, peg_whitelisted
                           ? nSupply
                           : 0);
        inputFees->setData(COL_INP_VALUE, Qt::TextAlignmentRole, int(Qt::AlignVCenter | Qt::AlignRight));
        inputFees->setData(COL_INP_VALUE, BlockchainModel::ValueForCopy, qlonglong(nFeesValue));
        ui->txInputs->addTopLevelItem(inputFees);
    }

    int nAlignInpHigh = 0;
    for(int i=0; i< ui->txInputs->topLevelItemCount(); i++) {
        auto input = ui->txInputs->topLevelItem(i);
        auto nSupply = input->data(COL_INP_FRACTIONS, BlockchainModel::PegSupplyRole).toInt();
        QVariant vfractions = input->data(COL_INP_FRACTIONS, BlockchainModel::FractionsRole);
        if (!vfractions.isValid()) continue;
        CFractions fractions = vfractions.value<CFractions>();
        int64_t liquidity = fractions.High(nSupply);
        QString text = displayValue(liquidity);
        if (text.length() > nAlignInpHigh)
            nAlignInpHigh = text.length();
    }
    for(int i=0; i< ui->txInputs->topLevelItemCount(); i++) {
        auto input = ui->txInputs->topLevelItem(i);
        int64_t nValue = input->data(COL_INP_VALUE, BlockchainModel::ValueForCopy).toLongLong();
        auto nSupply = input->data(COL_INP_FRACTIONS, BlockchainModel::PegSupplyRole).toInt();
        QVariant vfractions = input->data(COL_INP_FRACTIONS, BlockchainModel::FractionsRole);
        if (!vfractions.isValid()) continue;
        CFractions fractions = vfractions.value<CFractions>();
        if (fractions.Total() != nValue) continue;
        int64_t reserve = fractions.Low(nSupply);
        int64_t liquidity = fractions.High(nSupply);
        input->setData(COL_INP_FRACTIONS, Qt::TextAlignmentRole, int(Qt::AlignVCenter | Qt::AlignRight));
        input->setText(COL_INP_FRACTIONS, displayValue(reserve)+" / "+displayValueR(liquidity, nAlignInpHigh));
    }

    CTxIndex txindex;
    txdb.ReadTxIndex(hash, txindex);

    ui->txOutputs->clear();
    for (unsigned int i = 0; i < n_vout; i++)
    {
        QStringList row;
        row << QString::number(i); // 0, idx

        bool hasSpend = false;
        QString titleSpend;
        uint256 next_hash;
        uint next_outnum = 0;

        if (i < txindex.vSpent.size()) {
            CDiskTxPos & txpos = txindex.vSpent[i];
            CTransaction txSpend;
            if (txSpend.ReadFromDisk(txpos)) {
                int vin_idx =0;
                for(const CTxIn &txin : txSpend.vin) {
                    if (txin.prevout.hash == hash && txin.prevout.n == i) {
                        next_hash = txSpend.GetHash();
                        next_outnum = i;

                        QString next_thash = QString::fromStdString(next_hash.ToString());
                        QString next_txid_hash = next_thash.left(4)+"..."+next_thash.right(4);
                        QString next_txid_height = txId(txdb, next_hash);
                        QString next_txid = next_txid_height.isEmpty() ? next_txid_hash : next_txid_height;

                        row << QString("%1:%2").arg(next_txid).arg(vin_idx); // 1, spend
                        titleSpend = QString("%1:%2").arg(next_thash).arg(vin_idx);
                        hasSpend = true;
                    }
                    vin_idx++;
                }
            }
        }

        bool is_notary = false;
        auto addr = scriptToAddress(tx.vout[i].scriptPubKey, is_notary);

        if (!hasSpend) {
            if (is_notary) {
                row << "Notary/Burn"; // 1, spend
            }
            else row << ""; // 1, spend
        }

        if (addr.isEmpty())
            row << "N/A"; // 2, address
        else row << addr;

        int64_t nValue = tx.vout[i].nValue;
        row << displayValue(nValue); // 3, value

        bool fIndicateFrozen = false;
        auto output = new QTreeWidgetItem(row);
        if (hasSpend) {
            QVariant vhash;
            vhash.setValue(next_hash);
            output->setData(COL_OUT_TX, BlockchainModel::HashRole, vhash);
            output->setData(COL_OUT_TX, BlockchainModel::OutNumRole, next_outnum);
        }
        auto fkey = uint320(hash, i);
        if (mapFractionsUnused.find(fkey) != mapFractionsUnused.end()) {
            QVariant vFractions;
            vFractions.setValue(mapFractionsUnused[fkey]);
            output->setData(COL_OUT_FRACTIONS, BlockchainModel::HashRole, titleSpend);
            output->setData(COL_OUT_FRACTIONS, BlockchainModel::FractionsRole, vFractions);
            output->setData(COL_OUT_FRACTIONS, BlockchainModel::PegSupplyRole, nSupply);
            if (mapFractionsUnused[fkey].nFlags & CFractions::NOTARY_F) {
                output->setData(COL_OUT_VALUE, Qt::DecorationPropertyRole, pmNotaryF);
                fIndicateFrozen = true;
            }
            else if (mapFractionsUnused[fkey].nFlags & CFractions::NOTARY_V) {
                output->setData(COL_OUT_VALUE, Qt::DecorationPropertyRole, pmNotaryV);
                fIndicateFrozen = true;
            }
        }
        if (!peg_whitelisted) {
            QVariant vFractions;
            vFractions.setValue(CFractions(tx.vout[i].nValue, CFractions::STD));
            output->setData(COL_OUT_FRACTIONS, BlockchainModel::HashRole, titleSpend);
            output->setData(COL_OUT_FRACTIONS, BlockchainModel::FractionsRole, vFractions);
            output->setData(COL_OUT_FRACTIONS, BlockchainModel::PegSupplyRole, 0);
        }
        output->setData(COL_OUT_VALUE, Qt::TextAlignmentRole, int(Qt::AlignVCenter | Qt::AlignRight));
        output->setData(COL_OUT_VALUE, BlockchainModel::ValueForCopy, qlonglong(nValue));
        if (!fIndicateFrozen && sAddresses.contains(addr)) {
            output->setData(COL_OUT_VALUE, Qt::DecorationPropertyRole, pmChange);
        }
        ui->txOutputs->addTopLevelItem(output);
    }

    int nValueMaxLen = qMax(displayValue(nValueIn).length(),
                            qMax(displayValue(nValueOut).length(),
                                 qMax(displayValue(nReserveIn).length(),
                                      displayValue(nLiquidityIn).length())));
    auto sValueIn = displayValueR(nValueIn, nValueMaxLen);
    auto sValueOut = displayValueR(nValueOut, nValueMaxLen);
    auto sReserveIn = displayValueR(nReserveIn, nValueMaxLen);
    auto sLiquidityIn = displayValueR(nLiquidityIn, nValueMaxLen);

    auto twiValueIn = new QTreeWidgetItem(QStringList({"Value In",sValueIn}));
    auto twiValueOut = new QTreeWidgetItem(QStringList({"Value Out",sValueOut}));
    auto twiReserves = new QTreeWidgetItem(QStringList({"Reserves",sReserveIn}));
    auto twiLiquidity = new QTreeWidgetItem(QStringList({"Liquidity",sLiquidityIn}));

    twiValueIn->setData(1, BlockchainModel::ValueForCopy, qlonglong(nValueIn));
    twiValueOut->setData(1, BlockchainModel::ValueForCopy, qlonglong(nValueOut));
    twiReserves->setData(1, BlockchainModel::ValueForCopy, qlonglong(nReserveIn));
    twiLiquidity->setData(1, BlockchainModel::ValueForCopy, qlonglong(nLiquidityIn));

    ui->txValues->addTopLevelItem(twiValueIn);
    ui->txValues->addTopLevelItem(twiValueOut);
    ui->txValues->addTopLevelItem(twiReserves);
    ui->txValues->addTopLevelItem(twiLiquidity);

    auto twiPegChecks = new QTreeWidgetItem(
                QStringList({
                    "Peg Checks",
                    peg_whitelisted
                        ? peg_ok ? "OK" : "FAIL ("+QString::fromStdString(sPegFailCause)+")"
                        : "Not Whitelisted"
                })
    );
    ui->txValues->addTopLevelItem(twiPegChecks);
    ui->txValues->addTopLevelItem(new QTreeWidgetItem(QStringList({"Peg Checks Time",QString::number(msecsPegChecks)+" msecs"})));
    ui->txValues->addTopLevelItem(new QTreeWidgetItem(QStringList({"Fetch Inputs Time",QString::number(msecsFetchInputs)+" msecs (can be cached)"})));

    if (!tx.IsCoinBase() && !tx.IsCoinStake() /*&& nValueOut < nValueIn*/) {
        QStringList row;
        row << "Fee";
        row << ""; // spent
        row << ""; // address (todo)
        row << displayValue(nValueIn - nValueOut);
        auto outputFees = new QTreeWidgetItem(row);
        QVariant vfractions;
        if (peg_whitelisted) {
            vfractions.setValue(feesFractions);
            outputFees->setData(COL_OUT_FRACTIONS, BlockchainModel::FractionsRole, vfractions);
            outputFees->setData(COL_OUT_FRACTIONS, BlockchainModel::PegSupplyRole, nSupply);
        } else {
            vfractions.setValue(CFractions(nValueIn - nValueOut, CFractions::STD));
            outputFees->setData(COL_OUT_FRACTIONS, BlockchainModel::FractionsRole, vfractions);
            outputFees->setData(COL_OUT_FRACTIONS, BlockchainModel::PegSupplyRole, 0);
        }
        outputFees->setData(COL_OUT_VALUE, Qt::TextAlignmentRole, int(Qt::AlignVCenter | Qt::AlignRight));
        outputFees->setData(COL_OUT_VALUE, BlockchainModel::ValueForCopy, qlonglong(nValueIn - nValueOut));
        ui->txOutputs->addTopLevelItem(outputFees);
    }

    int nAlignOutHigh = 0;
    for(int i=0; i< ui->txOutputs->topLevelItemCount(); i++) {
        auto output = ui->txOutputs->topLevelItem(i);
        auto nSupply = output->data(COL_OUT_FRACTIONS, BlockchainModel::PegSupplyRole).toInt();
        QVariant vfractions = output->data(COL_OUT_FRACTIONS, BlockchainModel::FractionsRole);
        if (!vfractions.isValid()) continue;
        CFractions fractions = vfractions.value<CFractions>();
        int64_t liquidity = fractions.High(nSupply);
        QString text = displayValue(liquidity);
        if (text.length() > nAlignOutHigh)
            nAlignOutHigh = text.length();
    }
    for(int i=0; i< ui->txOutputs->topLevelItemCount(); i++) {
        auto output = ui->txOutputs->topLevelItem(i);
        int64_t nValue = output->data(COL_OUT_VALUE, BlockchainModel::ValueForCopy).toLongLong();
        auto nSupply = output->data(COL_OUT_FRACTIONS, BlockchainModel::PegSupplyRole).toInt();
        QVariant vfractions = output->data(COL_OUT_FRACTIONS, BlockchainModel::FractionsRole);
        if (!vfractions.isValid()) continue;
        CFractions fractions = vfractions.value<CFractions>();
        if (fractions.Total() != nValue) continue;
        int64_t reserve = fractions.Low(nSupply);
        int64_t liquidity = fractions.High(nSupply);
        output->setData(COL_OUT_FRACTIONS, Qt::TextAlignmentRole, int(Qt::AlignVCenter | Qt::AlignRight));
        output->setText(COL_OUT_FRACTIONS, displayValue(reserve)+" / "+displayValueR(liquidity, nAlignOutHigh));
    }

}

void TxDetailsWidget::openTxMenu(const QPoint & pos)
{
    QModelIndex mi = ui->txValues->indexAt(pos);
    if (!mi.isValid()) return;
    auto model = mi.model();
    if (!model) return;

    QMenu m;

    auto a = m.addAction(tr("Copy Value"));
    connect(a, &QAction::triggered, [&] {
        QString text;
        QModelIndex mi2 = model->index(mi.row(), 1 /*value column*/);
        QVariant v2 = mi2.data(BlockchainModel::ValueForCopy);
        if (v2.isValid())
            text = mi2.data(BlockchainModel::ValueForCopy).toString();
        else text = mi2.data(Qt::DisplayRole).toString();
        QApplication::clipboard()->setText(text);
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
    a = m.addAction(tr("Copy Transaction Info (json)"));
    connect(a, &QAction::triggered, [&] {
        QModelIndex mi2 = model->index(0, 0); // topItem
        auto hash = mi2.data(BlockchainModel::HashRole).value<uint256>();

        CTransaction tx;
        uint256 hashBlock = 0;
        if (!GetTransaction(hash, tx, hashBlock))
            return;

        int nSupply = -1;
        MapFractions mapFractions;
        {
            LOCK(cs_main);
            CPegDB pegdb("r");
            for(size_t i=0; i<tx.vout.size(); i++) {
                auto fkey = uint320(tx.GetHash(), i);
                CFractions fractions(0, CFractions::VALUE);
                pegdb.Read(fkey, fractions);
                if (fractions.Total() == tx.vout[i].nValue) {
                    mapFractions[fkey] = fractions;
                }
            }

            map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.find(hashBlock);
            if (mi != mapBlockIndex.end() && (*mi).second) {
                CBlockIndex* pindex = (*mi).second;
                nSupply = pindex->nPegSupplyIndex;
            }
        }

        json_spirit::Object result;
        TxToJSON(tx, hashBlock, mapFractions, nSupply, result);
        json_spirit::Value vresult = result;
        string str = json_spirit::write_string(vresult, true);

        QApplication::clipboard()->setText(
            QString::fromStdString(str)
        );
    });
    m.exec(ui->txValues->viewport()->mapToGlobal(pos));
}

void TxDetailsWidget::openInpMenu(const QPoint & pos)
{
    QModelIndex mi = ui->txInputs->indexAt(pos);
    if (!mi.isValid()) return;
    auto model = mi.model();
    if (!model) return;

    QMenu m;

    auto a = m.addAction(tr("Copy Value"));
    connect(a, &QAction::triggered, [&] {
        QModelIndex mi2 = model->index(mi.row(), COL_INP_VALUE);
        QApplication::clipboard()->setText(
            mi2.data(BlockchainModel::ValueForCopy).toString()
        );
    });
    a = m.addAction(tr("Copy Reserve"));
    connect(a, &QAction::triggered, [&] {
        QString text = "None";
        QModelIndex mi2 = model->index(mi.row(), COL_INP_VALUE);
        int64_t nValue = mi2.data(BlockchainModel::ValueForCopy).toLongLong();
        QModelIndex mi3 = model->index(mi.row(), COL_INP_FRACTIONS);
        int nSupply = mi3.data(BlockchainModel::PegSupplyRole).toInt();
        QVariant vfractions = mi3.data(BlockchainModel::FractionsRole);
        if (vfractions.isValid()) {
            CFractions fractions = vfractions.value<CFractions>();
            if (fractions.Total() == nValue) {
                int64_t nReserve = fractions.Low(nSupply);
                text = QString::number(nReserve);
            }
        }
        QApplication::clipboard()->setText(text);
    });
    a = m.addAction(tr("Copy Liquidity"));
    connect(a, &QAction::triggered, [&] {
        QString text = "None";
        QModelIndex mi2 = model->index(mi.row(), COL_INP_VALUE);
        int64_t nValue = mi2.data(BlockchainModel::ValueForCopy).toLongLong();
        QModelIndex mi3 = model->index(mi.row(), COL_INP_FRACTIONS);
        int nSupply = mi3.data(BlockchainModel::PegSupplyRole).toInt();
        QVariant vfractions = mi3.data(BlockchainModel::FractionsRole);
        if (vfractions.isValid()) {
            CFractions fractions = vfractions.value<CFractions>();
            if (fractions.Total() == nValue) {
                int64_t nLiquidity = fractions.High(nSupply);
                text = QString::number(nLiquidity);
            }
        }
        QApplication::clipboard()->setText(text);
    });
    m.addSeparator();
    a = m.addAction(tr("Copy Address"));
    connect(a, &QAction::triggered, [&] {
        QModelIndex mi2 = model->index(mi.row(), COL_INP_ADDR);
        QApplication::clipboard()->setText(
            mi2.data(Qt::DisplayRole).toString()
        );
    });
    a = m.addAction(tr("Copy Input Hash"));
    connect(a, &QAction::triggered, [&] {
        QString text = "None";
        QModelIndex mi2 = model->index(mi.row(), COL_INP_TX);
        QVariant vhash = mi2.data(BlockchainModel::HashRole);
        if (vhash.isValid()) {
            uint256 hash = vhash.value<uint256>();
            int n = mi2.data(BlockchainModel::OutNumRole).toInt();
            text = QString::fromStdString(hash.ToString());
            text += ":"+QString::number(n);
        }
        QApplication::clipboard()->setText(text);
    });
    a = m.addAction(tr("Copy Input Height"));
    connect(a, &QAction::triggered, [&] {
        QModelIndex mi2 = model->index(mi.row(), COL_INP_TX);
        QApplication::clipboard()->setText(
            mi2.data(Qt::DisplayRole).toString()
        );
    });
    m.addSeparator();
    a = m.addAction(tr("Copy Input Fractions"));
    connect(a, &QAction::triggered, [&] {
        QModelIndex mi2 = model->index(mi.row(), COL_INP_FRACTIONS);
        QVariant vfractions = mi2.data(BlockchainModel::FractionsRole);
        if (!vfractions.isValid()) return;
        CFractions fractions = vfractions.value<CFractions>().Std();
        QString text;
        for (int i=0; i<PEG_SIZE; i++) {
            if (i!=0) text += "\n";
            text += QString::number(i);
            text += "\t";
            text += QString::number(qlonglong(fractions.f[i]));
        }
        QApplication::clipboard()->setText(text);
    });

    m.exec(ui->txInputs->viewport()->mapToGlobal(pos));
}

void TxDetailsWidget::openOutMenu(const QPoint & pos)
{
    QModelIndex mi = ui->txOutputs->indexAt(pos);
    if (!mi.isValid()) return;
    auto model = mi.model();
    if (!model) return;

    QMenu m;

    auto a = m.addAction(tr("Copy Value"));
    connect(a, &QAction::triggered, [&] {
        QModelIndex mi2 = model->index(mi.row(), COL_OUT_VALUE);
        QApplication::clipboard()->setText(
            mi2.data(BlockchainModel::ValueForCopy).toString()
        );
    });
    a = m.addAction(tr("Copy Reserve"));
    connect(a, &QAction::triggered, [&] {
        QString text = "None";
        QModelIndex mi2 = model->index(mi.row(), COL_OUT_VALUE);
        int64_t nValue = mi2.data(BlockchainModel::ValueForCopy).toLongLong();
        QModelIndex mi3 = model->index(mi.row(), COL_OUT_FRACTIONS);
        int nSupply = mi3.data(BlockchainModel::PegSupplyRole).toInt();
        QVariant vfractions = mi3.data(BlockchainModel::FractionsRole);
        if (vfractions.isValid()) {
            CFractions fractions = vfractions.value<CFractions>();
            if (fractions.Total() == nValue) {
                int64_t nReserve = fractions.Low(nSupply);
                text = QString::number(nReserve);
            }
        }
        QApplication::clipboard()->setText(text);
    });
    a = m.addAction(tr("Copy Liquidity"));
    connect(a, &QAction::triggered, [&] {
        QString text = "None";
        QModelIndex mi2 = model->index(mi.row(), COL_OUT_VALUE);
        int64_t nValue = mi2.data(BlockchainModel::ValueForCopy).toLongLong();
        QModelIndex mi3 = model->index(mi.row(), COL_OUT_FRACTIONS);
        int nSupply = mi3.data(BlockchainModel::PegSupplyRole).toInt();
        QVariant vfractions = mi3.data(BlockchainModel::FractionsRole);
        if (vfractions.isValid()) {
            CFractions fractions = vfractions.value<CFractions>();
            if (fractions.Total() == nValue) {
                int64_t nLiquidity = fractions.High(nSupply);
                text = QString::number(nLiquidity);
            }
        }
        QApplication::clipboard()->setText(text);
    });
    m.addSeparator();
    a = m.addAction(tr("Copy Address"));
    connect(a, &QAction::triggered, [&] {
        QModelIndex mi2 = model->index(mi.row(), COL_OUT_ADDR);
        QApplication::clipboard()->setText(
            mi2.data(Qt::DisplayRole).toString()
        );
    });
    a = m.addAction(tr("Copy Spent Hash"));
    connect(a, &QAction::triggered, [&] {
        QString text = "None";
        QModelIndex mi2 = model->index(mi.row(), COL_OUT_TX);
        QVariant vhash = mi2.data(BlockchainModel::HashRole);
        if (vhash.isValid()) {
            uint256 hash = vhash.value<uint256>();
            int n = mi2.data(BlockchainModel::OutNumRole).toInt();
            text = QString::fromStdString(hash.ToString());
            text += ":"+QString::number(n);
        }
        QApplication::clipboard()->setText(text);
    });
    a = m.addAction(tr("Copy Spent Height"));
    connect(a, &QAction::triggered, [&] {
        QModelIndex mi2 = model->index(mi.row(), COL_OUT_TX);
        QApplication::clipboard()->setText(
            mi2.data(Qt::DisplayRole).toString()
        );
    });
    m.addSeparator();
    a = m.addAction(tr("Copy Spent Fractions"));
    connect(a, &QAction::triggered, [&] {
        QModelIndex mi2 = model->index(mi.row(), COL_OUT_FRACTIONS);
        QVariant vfractions = mi2.data(BlockchainModel::FractionsRole);
        if (!vfractions.isValid()) return;
        CFractions fractions = vfractions.value<CFractions>().Std();
        QString text;
        for (int i=0; i<PEG_SIZE; i++) {
            if (i!=0) text += "\n";
            text += QString::number(i);
            text += "\t";
            text += QString::number(qlonglong(fractions.f[i]));
        }
        QApplication::clipboard()->setText(text);
    });

    m.exec(ui->txOutputs->viewport()->mapToGlobal(pos));
}

bool TxDetailsWidgetTxEvents::eventFilter(QObject *obj, QEvent *event)
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

void TxDetailsWidget::openFractions(QTreeWidgetItem * item, int column)
{
    if (column != 4) // only fractions column
        return;

    auto dlg = new QDialog(this);
    Ui::FractionsDialog ui;
    ui.setupUi(dlg);
    QwtPlot * fplot = new QwtPlot;
    QVBoxLayout *fvbox = new QVBoxLayout;
    fvbox->setMargin(0);
    fvbox->addWidget(fplot);
    ui.chart->setLayout(fvbox);

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
    ui.fractions->setStyleSheet(hstyle);
    ui.fractions->setFont(font);
    ui.fractions->header()->setFont(font);
    ui.fractions->header()->resizeSection(0 /*n*/, 50);
    ui.fractions->header()->resizeSection(1 /*value*/, 160);

    ui.fractions->setContextMenuPolicy(Qt::CustomContextMenu);
    //ui.fractions->installEventFilter(new FractionsDialogEvents(ui.fractions, this));
    connect(ui.fractions, SIGNAL(customContextMenuRequested(const QPoint &)),
            this, SLOT(openFractionsMenu(const QPoint &)));

    auto txhash = item->data(4, BlockchainModel::HashRole).toString();
    auto supply = item->data(4, BlockchainModel::PegSupplyRole).toInt();
    auto vfractions = item->data(4, BlockchainModel::FractionsRole);
    auto fractions = vfractions.value<CFractions>();
    auto fractions_std = fractions.Std();

//    int64_t fdelta[CPegFractions::PEG_SIZE];
//    int64_t fundelta[CPegFractions::PEG_SIZE];
//    fractions_std.ToDeltas(fdelta);
//    CPegFractions fd;
//    fd.FromDeltas(fdelta);

    unsigned long len_test = 0;
    CDataStream fout_test(SER_DISK, CLIENT_VERSION);
    fractions.Pack(fout_test, &len_test);
    ui.packedLabel->setText(tr("Packed: %1 bytes").arg(len_test));
    ui.valueLabel->setText(tr("Value: %1").arg(displayValue(fractions.Total())));
    ui.reserveLabel->setText(tr("Reserve: %1").arg(displayValue(fractions.Low(supply))));
    ui.liquidityLabel->setText(tr("Liquidity: %1").arg(displayValue(fractions.High(supply))));

    qreal xs_reserve[PEG_SIZE*2];
    qreal ys_reserve[PEG_SIZE*2];
    qreal xs_liquidity[PEG_SIZE*2];
    qreal ys_liquidity[PEG_SIZE*2];

    for (int i=0; i<PEG_SIZE; i++) {
        QStringList row;
        row << QString::number(i) << displayValue(fractions_std.f[i]); // << QString::number(fdelta[i]) << QString::number(fd.f[i]);
        auto row_item = new QTreeWidgetItem(row);
        row_item->setData(0, Qt::TextAlignmentRole, int(Qt::AlignVCenter | Qt::AlignRight));
        row_item->setData(1, Qt::TextAlignmentRole, int(Qt::AlignVCenter | Qt::AlignRight));
        row_item->setData(1, BlockchainModel::ValueForCopy, qlonglong(fractions_std.f[i]));
        ui.fractions->addTopLevelItem(row_item);

        xs_reserve[i*2] = i;
        ys_reserve[i*2] = i < supply ? qreal(fractions_std.f[i]) : 0;
        xs_reserve[i*2+1] = i+1;
        ys_reserve[i*2+1] = ys_reserve[i*2];

        xs_liquidity[i*2] = i;
        ys_liquidity[i*2] = i >= supply ? qreal(fractions_std.f[i]) : 0;
        xs_liquidity[i*2+1] = i+1;
        ys_liquidity[i*2+1] = ys_liquidity[i*2];
    }

    QPen nopen(Qt::NoPen);

    auto curve_reserve = new QwtPlotCurve;
    curve_reserve->setPen(nopen);
    curve_reserve->setBrush(QColor("#c06a15"));
    curve_reserve->setSamples(xs_reserve, ys_reserve, supply*2);
    curve_reserve->setRenderHint(QwtPlotItem::RenderAntialiased);
    curve_reserve->attach(fplot);

    auto curve_liquidity = new QwtPlotCurve;
    curve_liquidity->setPen(nopen);
    curve_liquidity->setBrush(QColor("#2da5e0"));
    curve_liquidity->setSamples(xs_liquidity+supply*2,
                                ys_liquidity+supply*2,
                                PEG_SIZE*2-supply*2);
    curve_liquidity->setRenderHint(QwtPlotItem::RenderAntialiased);
    curve_liquidity->attach(fplot);

    fplot->replot();

    dlg->setWindowTitle(txhash+" "+tr("fractions"));
    dlg->show();
}

void TxDetailsWidget::openFractionsMenu(const QPoint & pos)
{
    QTreeWidget * table = dynamic_cast<QTreeWidget *>(sender());
    if (!table) return;
    QModelIndex mi = table->indexAt(pos);
    if (!mi.isValid()) return;
    auto model = mi.model();
    if (!model) return;

    QMenu m;

    auto a = m.addAction(tr("Copy Value"));
    connect(a, &QAction::triggered, [&] {
        QString text;
        QModelIndex mi2 = model->index(mi.row(), 1 /*value column*/);
        QVariant v1 = mi2.data(BlockchainModel::ValueForCopy);
        if (v1.isValid())
            text = v1.toString();
        else text = mi2.data(Qt::DisplayRole).toString();
        QApplication::clipboard()->setText(text);
    });
    a = m.addAction(tr("Copy All Rows"));
    connect(a, &QAction::triggered, [&] {
        QString text;
        for(int r=0; r<model->rowCount(); r++) {
            for(int c=0; c<model->columnCount(); c++) {
                if (c>0) text += "\t";
                QModelIndex mi2 = model->index(r, c);
                QVariant v1 = mi2.data(BlockchainModel::ValueForCopy);
                if (v1.isValid())
                    text += v1.toString();
                else text += mi2.data(Qt::DisplayRole).toString();
            }
            text += "\n";
        }
        QApplication::clipboard()->setText(text);
    });
    m.exec(table->viewport()->mapToGlobal(pos));
}

bool FractionsDialogEvents::eventFilter(QObject *obj, QEvent *event)
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
