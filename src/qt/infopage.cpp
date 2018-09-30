#include "infopage.h"
#include "ui_infopage.h"

#include "main.h"
#include "base58.h"
#include "txdb-leveldb.h"
#include "guiutil.h"
#include "blockchainmodel.h"
#include "metatypes.h"

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
    
    connect(ui->buttonChain, SIGNAL(clicked()), this, SLOT(showChainPage()));
    connect(ui->buttonBlock, SIGNAL(clicked()), this, SLOT(showBlockPage()));
    connect(ui->buttonTx, SIGNAL(clicked()), this, SLOT(showTxPage()));
    
    connect(ui->blockchainView, SIGNAL(doubleClicked(const QModelIndex &)),
            this, SLOT(openBlock(const QModelIndex &)));
    connect(ui->blockValues, SIGNAL(itemDoubleClicked(QTreeWidgetItem*,int)),
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
    ui->blockValues->setStyleSheet(hstyle);
    
    ui->txValues->setFont(font);
    ui->txInputs->setFont(font);
    ui->txOutputs->setFont(font);
    ui->blockValues->setFont(font);
    
    ui->txValues->header()->setFont(font);
    ui->txInputs->header()->setFont(font);
    ui->txOutputs->header()->setFont(font);
    ui->blockValues->header()->setFont(font);
    
    ui->txInputs->header()->resizeSection(0 /*n*/, 50);
    ui->txOutputs->header()->resizeSection(0 /*n*/, 50);
    ui->txInputs->header()->resizeSection(1 /*tx*/, 140);
    ui->txInputs->header()->resizeSection(2 /*addr*/, 280);
    ui->txOutputs->header()->resizeSection(1 /*addr*/, 280);
    ui->txInputs->header()->resizeSection(3 /*value*/, 160);
    ui->txOutputs->header()->resizeSection(2 /*value*/, 160);

    auto txInpDelegate = new FractionsItemDelegate(ui->txInputs);
    ui->txInputs->setItemDelegateForColumn(4 /*frac*/, txInpDelegate);
    
    auto txOutDelegate = new FractionsItemDelegate(ui->txOutputs);
    ui->txOutputs->setItemDelegateForColumn(3 /*frac*/, txOutDelegate);
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

void InfoPage::openBlock(const QModelIndex & mi)
{
    if (!mi.isValid())
        return;
    currentBlock = mi.data(BlockchainModel::HashRole).value<uint256>();
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
    int tx_idx = item->text(0).mid(2).toInt(&tx_idx_ok);
    if (!tx_idx_ok)
        return;
    if (tx_idx <0)
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
    if (tx_idx >= int(block.vtx.size()))
        return;
    
    CTransaction & tx = block.vtx[(unsigned int)tx_idx];
    uint256 hash = tx.GetHash();
    QString thash = QString::fromStdString(hash.ToString());
    QString sheight = QString("%1:%2").arg(pblockindex->nHeight).arg(tx_idx);

    CTxDB txdb("r");
    if (!txdb.ContainsTx(hash))
        return;
    
    showTxPage();
    ui->txValues->clear();
    ui->txValues->addTopLevelItem(new QTreeWidgetItem(QStringList({"Height",sheight})));
    ui->txValues->addTopLevelItem(new QTreeWidgetItem(QStringList({"Hash",thash})));
        
    MapPrevTx mapInputs;
    map<uint256, CTxIndex> mapUnused;
    bool fInvalid = false;
    tx.FetchInputs(txdb, mapUnused, false, false, mapInputs, fInvalid);
    
    ui->txInputs->clear();
    for (unsigned int i = 0; i < tx.vin.size(); i++)
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
                    row << "not decoded"; // address
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
        
        ui->txInputs->addTopLevelItem(new QTreeWidgetItem(row));
    }

    ui->txOutputs->clear();
    for (unsigned int i = 0; i < tx.vout.size(); i++)
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
            row << "not decoded"; // address
        }
        
        int64_t value = tx.vout[i].nValue;
        row << QString::number(value);
        
        ui->txOutputs->addTopLevelItem(new QTreeWidgetItem(row));
    }
}

// delegate to draw fractions

FractionsItemDelegate::FractionsItemDelegate(QWidget *parent) :
    QItemDelegate(parent)
{
}

FractionsItemDelegate::~FractionsItemDelegate()
{
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

void FractionsItemDelegate::drawDisplay(QPainter *p, 
                                        const QStyleOptionViewItem &o, 
                                        const QRect &r, 
                                        const QString &t) const
{
    Q_UNUSED(o);
    Q_UNUSED(t);
    
    int64_t fs[1200];
    int64_t v = 10000000000;
    value_to_fractions(v, fs);
    
    QPainterPath path;
    QVector<QPointF> points;
    
    qreal rx = r.x();
    qreal ry = r.y();
    qreal rw = r.width();
    qreal rh = r.height();
    qreal w = 1200;
    qreal h = fs[0];
    
    points.push_back(QPointF(r.x(),r.bottom()));
    
    for (int i=0; i<1200; i++) {
        qreal x = rx + qreal(i)*rw/w;
        qreal y = ry + rh - qreal(fs[i])*rh/h;
        points.push_back(QPointF(x,y));
    }

    QPolygonF poly(points);
    path.addPolygon(poly);
    p->setRenderHint( QPainter::Antialiasing );
    p->setBrush( Qt::blue );
    p->setPen( Qt::darkBlue );
    p->drawPath( path );
    
    p->setPen( Qt::darkGreen );
    qreal pegx = rx + 150*rw/w; // test
    p->drawLine(QPointF(pegx, ry), QPointF(pegx, ry+rh));
}
