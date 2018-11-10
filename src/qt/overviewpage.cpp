#include "overviewpage.h"
#include "ui_overviewpage.h"

#include "clientmodel.h"
#include "walletmodel.h"
#include "bitcoinunits.h"
#include "optionsmodel.h"
#include "transactiontablemodel.h"
#include "transactionfilterproxy.h"
#include "guiutil.h"
#include "guiconstants.h"
#include "peg.h"

#include "qwt/qwt_plot.h"
#include "qwt/qwt_plot_curve.h"
#include "qwt/qwt_plot_barchart.h"
#include "qwt/qwt_plot_layout.h"
#include "qwt/qwt_scale_widget.h"

#include <QAbstractItemDelegate>
#include <QPainter>

#define DECORATION_SIZE 64
#define NUM_ITEMS 6

class TxViewDelegate : public QAbstractItemDelegate
{
    Q_OBJECT
public:
    TxViewDelegate(): QAbstractItemDelegate(), unit(BitcoinUnits::BTC)
    {

    }

    inline void paint(QPainter *painter, const QStyleOptionViewItem &option,
                      const QModelIndex &index ) const
    {
        painter->save();

        QIcon icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
        QRect mainRect = option.rect;
//        QRect decorationRect(mainRect.topLeft(), QSize(DECORATION_SIZE, DECORATION_SIZE));
//        int xspace = DECORATION_SIZE + 8;
        int xspace = 0;
        int ypad = 6;
        //int halfheight = (mainRect.height() - 2*ypad)/2;
        int halfheight = (mainRect.height() - 2*ypad);
        QRect amountRect(mainRect.left() + xspace, mainRect.top()+ypad, mainRect.width() - xspace, halfheight);
        //QRect addressRect(mainRect.left() + xspace, mainRect.top()+ypad+halfheight, mainRect.width() - xspace, halfheight);
        //icon.paint(painter, decorationRect);

        QDateTime date = index.data(TransactionTableModel::DateRole).toDateTime();
        QString address = index.data(Qt::DisplayRole).toString();
        qint64 amount = index.data(TransactionTableModel::AmountRole).toLongLong();
        bool confirmed = index.data(TransactionTableModel::ConfirmedRole).toBool();
        QVariant value = index.data(Qt::ForegroundRole);
        QColor foreground = option.palette.color(QPalette::Text);
        if(qVariantCanConvert<QColor>(value))
        {
            foreground = qvariant_cast<QColor>(value);
        }

        painter->setPen(foreground);
        //painter->drawText(addressRect, Qt::AlignLeft|Qt::AlignVCenter, address);

        if(amount < 0)
        {
            foreground = COLOR_NEGATIVE;
        }
        else if(!confirmed)
        {
            foreground = COLOR_UNCONFIRMED;
        }
        else
        {
            foreground = option.palette.color(QPalette::Text);
        }
        painter->setPen(foreground);
        QString amountText = BitcoinUnits::formatWithUnit(unit, amount, true);
        if(!confirmed)
        {
            amountText = QString("[") + amountText + QString("]");
        }
        QFont f = painter->font();
        QFont fb = f;
        fb.setBold(true);
        painter->setFont(fb);
        painter->drawText(amountRect, Qt::AlignRight|Qt::AlignVCenter, amountText);

        painter->setFont(f);
        painter->setPen(QPen("#666666"));
        painter->drawText(amountRect, Qt::AlignLeft|Qt::AlignVCenter, GUIUtil::dateTimeStr(date));

        painter->restore();
    }

    inline QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        //return QSize(DECORATION_SIZE, DECORATION_SIZE);
        return QSize(DECORATION_SIZE, DECORATION_SIZE/2);
    }

    int unit;

};
#include "overviewpage.moc"

OverviewPage::OverviewPage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::OverviewPage),
    clientModel(0),
    walletModel(0),
    currentBalance(-1),
    currentReserve(-1),
    currentLiquidity(-1),
    currentStake(0),
    currentUnconfirmedBalance(-1),
    currentImmatureBalance(-1),
    txdelegate(new TxViewDelegate()),
    filter(0)
{
    ui->setupUi(this);
    GUIUtil::SetBitBayFonts(this);

    // Recent transactions
    ui->listTransactions->setItemDelegate(txdelegate);
    ui->listTransactions->setIconSize(QSize(DECORATION_SIZE, DECORATION_SIZE));
    ui->listTransactions->setMinimumHeight(NUM_ITEMS * (DECORATION_SIZE + 2));
    ui->listTransactions->setAttribute(Qt::WA_MacShowFocusRect, false);

#ifdef Q_OS_MAC
    QFont hfont("Roboto Black", 20, QFont::Bold);
#else
    QFont hfont("Roboto Black", 15, QFont::Bold);
#endif

    ui->labelWallet->setFont(hfont);
    ui->labelRecent->setFont(hfont);

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

    ui->w_recent->setStyleSheet(white1);

    ui->labelBalanceText        ->setStyleSheet(white2);
    ui->labelReserveText        ->setStyleSheet(white2);
    ui->labelLiquidityText      ->setStyleSheet(white2);
    ui->labelStakeText          ->setStyleSheet(white2);
    ui->labelUnconfirmedText    ->setStyleSheet(white2);
    ui->labelImmatureText       ->setStyleSheet(white2);

#ifdef Q_OS_MAC
    QFont tfont("Roboto", 15, QFont::Bold);
#else
    QFont tfont("Roboto", 11, QFont::Bold);
#endif
    ui->labelTotalText->setFont(tfont);
    ui->labelTotalText          ->setStyleSheet(white1);

    ui->labelBalance            ->setStyleSheet(white1);
    ui->labelReserve            ->setStyleSheet(white1);
    ui->labelLiquidity          ->setStyleSheet(white1);
    ui->labelStake              ->setStyleSheet(white1);
    ui->labelUnconfirmed        ->setStyleSheet(white1);
    ui->labelImmature           ->setStyleSheet(white1);
    ui->labelTotal              ->setStyleSheet(white1);

    connect(ui->listTransactions, SIGNAL(clicked(QModelIndex)), this, SLOT(handleTransactionClicked(QModelIndex)));

    // init "out of sync" warning labels
    ui->labelWalletStatus->setText("(" + tr("out of sync") + ")");
    ui->labelTransactionsStatus->setText("(" + tr("out of sync") + ")");

    // start with displaying the "out of sync" warnings
    showOutOfSyncWarning(true);

    // temp, show fractions
    {
        QwtPlot * fplot = new QwtPlot;
        QVBoxLayout *fvbox = new QVBoxLayout;
        fvbox->setMargin(0);
        fvbox->addWidget(fplot);
        ui->widgetFractions->setLayout(fvbox);

        auto supply = 40; // temp
        auto fractions = CFractions(100*100000000, CFractions::STD);
        auto fractions_std = fractions.Std();

        qreal xs_reserve[PEG_SIZE*2];
        qreal ys_reserve[PEG_SIZE*2];
        qreal xs_liquidity[PEG_SIZE*2];
        qreal ys_liquidity[PEG_SIZE*2];

        for (int i=0; i<PEG_SIZE; i++) {
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

        fplot->enableAxis(0, false);
        fplot->enableAxis(1, false);
        fplot->enableAxis(2, false);
        fplot->plotLayout()->setCanvasMargin(0);
        fplot->replot();
    }
}

void OverviewPage::handleTransactionClicked(const QModelIndex &index)
{
    if(filter)
        emit transactionClicked(filter->mapToSource(index));
}

OverviewPage::~OverviewPage()
{
    delete ui;
}

void OverviewPage::setBalance(qint64 balance, qint64 reserve, qint64 liquidity, 
                              qint64 stake, qint64 unconfirmedBalance, qint64 immatureBalance)
{
    int unit = walletModel->getOptionsModel()->getDisplayUnit();
    currentBalance = balance;
    currentReserve = reserve;
    currentLiquidity = liquidity;
    currentStake = stake;
    currentUnconfirmedBalance = unconfirmedBalance;
    currentImmatureBalance = immatureBalance;
    ui->labelBalance->setText(BitcoinUnits::formatWithUnitForLabel(unit, balance));
    ui->labelReserve->setText(BitcoinUnits::formatWithUnitForLabel(unit, reserve));
    ui->labelLiquidity->setText(BitcoinUnits::formatWithUnitForLabel(unit, liquidity));
    ui->labelStake->setText(BitcoinUnits::formatWithUnitForLabel(unit, stake));
    ui->labelUnconfirmed->setText(BitcoinUnits::formatWithUnitForLabel(unit, unconfirmedBalance));
    ui->labelImmature->setText(BitcoinUnits::formatWithUnitForLabel(unit, immatureBalance));
    ui->labelTotal->setText(BitcoinUnits::formatWithUnitForLabel(unit, balance + stake + unconfirmedBalance + immatureBalance));

    // only show immature (newly mined) balance if it's non-zero, so as not to complicate things
    // for the non-mining users
    bool showImmature = immatureBalance != 0;
    ui->labelImmature->setVisible(showImmature);
    ui->labelImmatureText->setVisible(showImmature);
}

void OverviewPage::setClientModel(ClientModel *model)
{
    this->clientModel = model;
    if(model)
    {
        // Show warning if this is a prerelease version
        connect(model, SIGNAL(alertsChanged(QString)), this, SLOT(updateAlerts(QString)));
        updateAlerts(model->getStatusBarWarnings());
    }
}

void OverviewPage::setWalletModel(WalletModel *model)
{
    this->walletModel = model;
    if(model && model->getOptionsModel())
    {
        // Set up transaction list
        filter = new TransactionFilterProxy();
        filter->setSourceModel(model->getTransactionTableModel());
        filter->setLimit(NUM_ITEMS);
        filter->setDynamicSortFilter(true);
        filter->setSortRole(Qt::EditRole);
        filter->setShowInactive(false);
        filter->sort(TransactionTableModel::Status, Qt::DescendingOrder);

        ui->listTransactions->setModel(filter);
        ui->listTransactions->setModelColumn(TransactionTableModel::ToAddress);

        // Keep up to date with wallet
        setBalance(model->getBalance(), model->getReserve(), model->getLiquidity(),
                   model->getStake(), model->getUnconfirmedBalance(), model->getImmatureBalance());
        connect(model, SIGNAL(balanceChanged(qint64, qint64, qint64, qint64, qint64, qint64)), 
                this, SLOT(setBalance(qint64, qint64, qint64, qint64, qint64, qint64)));

        connect(model->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(updateDisplayUnit()));
    }

    // update the display unit, to not use the default ("BTC")
    updateDisplayUnit();
}

void OverviewPage::updateDisplayUnit()
{
    if(walletModel && walletModel->getOptionsModel())
    {
        if(currentBalance != -1)
            setBalance(currentBalance, currentReserve, currentLiquidity,
                       walletModel->getStake(), currentUnconfirmedBalance, currentImmatureBalance);

        // Update txdelegate->unit with the current unit
        txdelegate->unit = walletModel->getOptionsModel()->getDisplayUnit();

        ui->listTransactions->update();
    }
}

void OverviewPage::updateAlerts(const QString &warnings)
{
    this->ui->labelAlerts->setVisible(!warnings.isEmpty());
    this->ui->labelAlerts->setText(warnings);
}

void OverviewPage::showOutOfSyncWarning(bool fShow)
{
    ui->labelWalletStatus->setVisible(fShow);
    ui->labelTransactionsStatus->setVisible(fShow);
}
