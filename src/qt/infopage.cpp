#include "infopage.h"
#include "ui_infopage.h"
#include "ui_fractionsdialog.h"

#include "main.h"
#include "base58.h"
#include "txdb.h"
#include "peg.h"
#include "guiutil.h"
#include "blockchainmodel.h"
#include "metatypes.h"
#include "qwt/qwt_plot.h"
#include "qwt/qwt_plot_curve.h"
#include "qwt/qwt_plot_barchart.h"

#include <QPainter>
#include <QClipboard>

#include <string>
#include <vector>

InfoPage::InfoPage(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::InfoPage)
{
    ui->setupUi(this);
    GUIUtil::SetBitBayFonts(this);

    model = new BlockchainModel(this);
    ui->blockchainView->setModel(model);

    connect(model, SIGNAL(rowsAboutToBeInserted(const QModelIndex &,int,int)),
            this, SLOT(updateCurrentBlockIndex()));
    connect(model, SIGNAL(rowsInserted(const QModelIndex &,int,int)),
            this, SLOT(scrollToCurrentBlockIndex()));

    connect(ui->buttonChain, SIGNAL(clicked()), this, SLOT(showChainPage()));
    connect(ui->buttonBlock, SIGNAL(clicked()), this, SLOT(showBlockPage()));
    connect(ui->buttonTx, SIGNAL(clicked()), this, SLOT(showTxPage()));

    connect(ui->blockchainView, SIGNAL(doubleClicked(const QModelIndex &)),
            this, SLOT(openBlock(const QModelIndex &)));
    connect(ui->blockValues, SIGNAL(itemDoubleClicked(QTreeWidgetItem*,int)),
            this, SLOT(openTx(QTreeWidgetItem*,int)));
    connect(ui->txInputs, SIGNAL(itemDoubleClicked(QTreeWidgetItem*,int)),
            this, SLOT(openFractions(QTreeWidgetItem*,int)));
    connect(ui->txOutputs, SIGNAL(itemDoubleClicked(QTreeWidgetItem*,int)),
            this, SLOT(openFractions(QTreeWidgetItem*,int)));

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
    ui->blockValues->setStyleSheet(hstyle);
    ui->blockchainView->setStyleSheet(hstyle);

    ui->txValues->setFont(font);
    ui->txInputs->setFont(font);
    ui->txOutputs->setFont(font);
    ui->blockValues->setFont(font);
    ui->blockchainView->setFont(font);

    ui->txValues->header()->setFont(font);
    ui->txInputs->header()->setFont(font);
    ui->txOutputs->header()->setFont(font);
    ui->blockValues->header()->setFont(font);
    ui->blockchainView->header()->setFont(font);

    ui->txInputs->header()->resizeSection(0 /*n*/, 50);
    ui->txOutputs->header()->resizeSection(0 /*n*/, 50);
    ui->txInputs->header()->resizeSection(1 /*tx*/, 140);
    ui->txInputs->header()->resizeSection(2 /*addr*/, 280);
    ui->txOutputs->header()->resizeSection(1 /*addr*/, 280);
    ui->txInputs->header()->resizeSection(3 /*value*/, 160);
    ui->txOutputs->header()->resizeSection(2 /*value*/, 160);
    ui->txOutputs->header()->resizeSection(3 /*spent*/, 140);

    auto txInpDelegate = new FractionsItemDelegate(ui->txInputs);
    ui->txInputs->setItemDelegateForColumn(4 /*frac*/, txInpDelegate);

    auto txOutDelegate = new FractionsItemDelegate(ui->txOutputs);
    ui->txOutputs->setItemDelegateForColumn(4 /*frac*/, txOutDelegate);

    connect(ui->lineJumpToBlock, SIGNAL(returnPressed()),
            this, SLOT(jumpToBlock()));
    connect(ui->lineFindBlock, SIGNAL(returnPressed()),
            this, SLOT(openBlockFromInput()));
}

InfoPage::~InfoPage()
{
    delete ui;
}

BlockchainModel * InfoPage::blockchainModel() const
{
    return model;
}

void InfoPage::showChainPage()
{
    ui->tabs->setCurrentWidget(ui->pageChain);
}

void InfoPage::showBlockPage()
{
    ui->tabs->setCurrentWidget(ui->pageBlock);
}

void InfoPage::showTxPage()
{
    ui->tabs->setCurrentWidget(ui->pageTx);
}

void InfoPage::jumpToBlock()
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
    ui->blockchainView->scrollTo(mi);
}

void InfoPage::openBlockFromInput()
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

void InfoPage::updateCurrentBlockIndex()
{
    currentBlockIndex = ui->blockchainView->currentIndex();
}

void InfoPage::scrollToCurrentBlockIndex()
{
    ui->blockchainView->scrollTo(currentBlockIndex);
}

void InfoPage::openBlock(const QModelIndex & mi)
{
    if (!mi.isValid())
        return;
    openBlock(mi.data(BlockchainModel::HashRole).value<uint256>());
}

void InfoPage::openBlock(uint256 hash)
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
    ui->blockValues->clear();
    ui->blockValues->addTopLevelItem(new QTreeWidgetItem(QStringList({"Height",QString::number(pblockindex->nHeight)})));
    ui->blockValues->addTopLevelItem(new QTreeWidgetItem(QStringList({"Hash",bhash})));

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

void InfoPage::openTx(QTreeWidgetItem * item, int column)
{
    Q_UNUSED(column);
    if (!item->text(0).startsWith("tx"))
        return;
    bool tx_idx_ok = false;
    uint tx_idx = item->text(0).mid(2).toUInt(&tx_idx_ok);
    if (!tx_idx_ok)
        return;

    //QString thash = item->text(1);

    LOCK(cs_main);
    if (mapBlockIndex.find(currentBlock) == mapBlockIndex.end())
        return;
    CBlockIndex* pblockindex = mapBlockIndex[currentBlock];
    if (!pblockindex)
        return;

    CBlock block;
    block.ReadFromDisk(pblockindex, true);
    if (tx_idx >= block.vtx.size())
        return;

    CTransaction & tx = block.vtx[tx_idx];
    uint256 hash = tx.GetHash();
    QString thash = QString::fromStdString(hash.ToString());
    QString sheight = QString("%1:%2").arg(pblockindex->nHeight).arg(tx_idx);

    CTxDB txdb("r");
    CPegDB pegdb("r");
    if (!txdb.ContainsTx(hash))
        return;

    showTxPage();
    ui->txValues->clear();
    ui->txValues->addTopLevelItem(new QTreeWidgetItem(QStringList({"Height",sheight})));
    ui->txValues->addTopLevelItem(new QTreeWidgetItem(QStringList({"Hash",thash})));

    MapPrevTx mapInputs;
    MapPrevFractions mapInputsFractions;
    map<uint256, CTxIndex> mapUnused;
    map<uint320, CPegFractions> mapFractionsUnused;
    bool fInvalid = false;
    tx.FetchInputs(txdb, pegdb, mapUnused, mapFractionsUnused, false, false, mapInputs, mapInputsFractions, fInvalid);

    ui->txInputs->clear();
    size_t n_vin = tx.vin.size();
    if (tx.IsCoinBase()) n_vin = 0;
    for (unsigned int i = 0; i < n_vin; i++)
    {
        COutPoint prevout = tx.vin[i].prevout;
        QStringList row;
        row << QString::number(i);

        QString prev_thash = QString::fromStdString(prevout.hash.ToString());
        QString sprev_thash = prev_thash.left(4)+"..."+prev_thash.right(4);
        row << QString("%1:%2").arg(sprev_thash).arg(prevout.n);

        if (mapInputs.find(prevout.hash) != mapInputs.end()) {
            CTransaction& txPrev = mapInputs[prevout.hash].second;
            if (prevout.n < txPrev.vout.size()) {
                int nRequired;
                txnouttype type;
                vector<CTxDestination> addresses;
                const CScript& scriptPubKey = txPrev.vout[prevout.n].scriptPubKey;
                if (ExtractDestinations(scriptPubKey, type, addresses, nRequired)) {
                    std::string str_addr_all;
                    for(const CTxDestination& addr : addresses) {
                        std::string str_addr = CBitcoinAddress(addr).ToString();
                        if (!str_addr_all.empty())
                            str_addr_all += "\n";
                        str_addr_all += str_addr;
                    }
                    row << QString::fromStdString(str_addr_all);
                }
                else {
                    row << "N/A"; // address
                }

                int64_t value = txPrev.vout[prevout.n].nValue;
                row << QString::number(value);
            }
            else {
                row << "none"; // address
                row << "none"; // value
            }
        }
        else {
            row << "none"; // address
            row << "none"; // value
        }

        auto input = new QTreeWidgetItem(row);
        auto fkey = uint320(prevout.hash, prevout.n);
        if (mapInputsFractions.find(fkey) != mapInputsFractions.end()) {
            QVariant vfractions;
            vfractions.setValue(mapInputsFractions[fkey]);
            input->setData(4, BlockchainModel::FractionsRole, vfractions);
            input->setData(4, BlockchainModel::PegSupplyRole, pblockindex->nPegSupplyIndex);
        }
        ui->txInputs->addTopLevelItem(input);
    }

    CalculateTransactionFractions(tx, pblockindex,
                                  mapInputs, mapInputsFractions,
                                  mapUnused, mapFractionsUnused);

    CTxIndex txindex;
    txdb.ReadTxIndex(hash, txindex);

    ui->txOutputs->clear();
    size_t n_vout = tx.vout.size();
    if (tx.IsCoinBase()) n_vout = 0;
    for (unsigned int i = 0; i < n_vout; i++)
    {
        QStringList row;
        row << QString::number(i);

        int nRequired;
        txnouttype type;
        vector<CTxDestination> addresses;
        const CScript& scriptPubKey = tx.vout[i].scriptPubKey;
        if (ExtractDestinations(scriptPubKey, type, addresses, nRequired)) {
            std::string str_addr_all;
            for(const CTxDestination& addr : addresses) {
                std::string str_addr = CBitcoinAddress(addr).ToString();
                if (str_addr == "bNyZrPLQAMPvYedrVLDcBSd8fbLdNgnRPz") {
                    str_addr = "peginflate";
                }
                else if (str_addr == "bNyZrP2SbrV6v5HqeBoXZXZDE2e4fe6STo") {
                    str_addr = "pegdeflate";
                }
                else if (str_addr == "bNyZrPeFFNP6GFJZCkE82DDN7JC4K5Vrkk") {
                    str_addr = "pegnochange";
                }
                if (!str_addr_all.empty())
                    str_addr_all += "\n";
                str_addr_all += str_addr;
            }
            row << QString::fromStdString(str_addr_all);
        }
        else {
            row << "N/A"; // address
        }

        int64_t value = tx.vout[i].nValue;
        row << QString::number(value);

        if (i < txindex.vSpent.size()) {
            CDiskTxPos & txpos = txindex.vSpent[i];
            CTransaction txSpend;
            if (txSpend.ReadFromDisk(txpos)) {
                int vin_idx =0;
                for(const CTxIn &txin : txSpend.vin) {
                    if (txin.prevout.hash == hash && txin.prevout.n == i) {
                        uint256 hashSpend = txSpend.GetHash();
                        QString shashSpend = QString::fromStdString(hashSpend.ToString());
                        QString shashSpendElided = shashSpend.left(4)+"..."+shashSpend.right(4);
                        row << QString("%1:%2").arg(shashSpendElided).arg(vin_idx);
                    }
                    vin_idx++;
                }
            }
        }

        auto output = new QTreeWidgetItem(row);
        auto fkey = uint320(hash, i);
        if (mapFractionsUnused.find(fkey) != mapFractionsUnused.end()) {
            QVariant vFractions;
            vFractions.setValue(mapFractionsUnused[fkey]);
            output->setData(4, BlockchainModel::FractionsRole, vFractions);
            output->setData(4, BlockchainModel::PegSupplyRole, pblockindex->nPegSupplyIndex);
        }
        ui->txOutputs->addTopLevelItem(output);
    }
}

static void value_to_fractions(int64_t v, int64_t *fs) {
    int64_t vf = 0;
    for(int i=0;i<1200;i++) {
        int64_t f = v/100;
        int64_t fm = v % 100;
        if (fm >= 45) f++;
        v -= f;
        vf += f;
        fs[i] = f;
    }
    int64_t r = v-vf;
    for(int i=0;i<r;i++) {
        fs[i]++;
    }
}

void InfoPage::openFractions(QTreeWidgetItem * item,int)
{
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

    auto vfractions = item->data(4, BlockchainModel::FractionsRole);
    auto fractions = vfractions.value<CPegFractions>();
    auto fractions_std = fractions.Std();

    int64_t f_max = 0;
    for (int i=0; i<CPegFractions::PEG_SIZE; i++) {
        auto f = fractions_std.f[i];
        if (f > f_max) f_max = f;
    }
    //if (f_max == 0)
    //    return; // zero-value fractions

    qreal xs[1200];
    qreal ys[1200];
    QVector<qreal> bs;
    for (int i=0; i<CPegFractions::PEG_SIZE; i++) {
        QStringList row;
        row << QString::number(i) << QString::number(fractions_std.f[i]);
        ui.fractions->addTopLevelItem(new QTreeWidgetItem(row));
        xs[i] = i;
        ys[i] = qreal(fractions_std.f[i]);
        bs.push_back(qreal(fractions_std.f[i]));
    }

    auto curve = new QwtPlotBarChart;
    //curve->setSamples(xs, ys, 1200);
    curve->setSamples(bs);
    curve->attach(fplot);
    fplot->replot();

    dlg->show();
}

// delegate to draw fractions

FractionsItemDelegate::FractionsItemDelegate(QWidget *parent) :
    QItemDelegate(parent)
{
}

FractionsItemDelegate::~FractionsItemDelegate()
{
}

void FractionsItemDelegate::paint(QPainter* p,
                                  const QStyleOptionViewItem& o,
                                  const QModelIndex& index) const
{
    auto vfractions = index.data(BlockchainModel::FractionsRole);
    auto fractions = vfractions.value<CPegFractions>();
    auto fractions_std = fractions.Std();

    int64_t f_max = 0;
    for (int i=0; i<CPegFractions::PEG_SIZE; i++) {
        auto f = fractions_std.f[i];
        if (f > f_max) f_max = f;
    }
    if (f_max == 0)
        return; // zero-value fractions

    auto supply = index.data(BlockchainModel::PegSupplyRole).toInt();

    QPainterPath path;
    QVector<QPointF> points;

    QRect r = o.rect;
    qreal rx = r.x();
    qreal ry = r.y();
    qreal rw = r.width();
    qreal rh = r.height();
    qreal w = CPegFractions::PEG_SIZE;
    qreal h = f_max;

    points.push_back(QPointF(r.x(),r.bottom()));

    for (int i=0; i<CPegFractions::PEG_SIZE; i++) {
        int64_t f = fractions_std.f[i];
        qreal x = rx + qreal(i)*rw/w;
        qreal y = ry + rh - qreal(f)*rh/h;
        points.push_back(QPointF(x,y));
    }

    QPolygonF poly(points);
    path.addPolygon(poly);
    p->setRenderHint( QPainter::Antialiasing );
    p->setBrush( Qt::blue );
    p->setPen( Qt::darkBlue );
    p->drawPath( path );

    p->setPen( Qt::darkGreen );
    qreal pegx = rx + supply*rw/w;
    p->drawLine(QPointF(pegx, ry), QPointF(pegx, ry+rh));
}
