#include "infopage.h"
#include "ui_infopage.h"

//#include "addressbookpage.h"
//#include "base58.h"
#include "guiutil.h"
//#include "init.h"
//#include "main.h"
//#include "optionsmodel.h"
//#include "walletmodel.h"
//#include "wallet.h"

#include <QClipboard>

#include <string>
#include <vector>

InfoPage::InfoPage(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::InfoPage)
{
    ui->setupUi(this);
    GUIUtil::SetBitBayFonts(this);
}

InfoPage::~InfoPage()
{
    delete ui;
}

