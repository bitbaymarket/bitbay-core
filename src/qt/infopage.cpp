#include "infopage.h"
#include "ui_infopage.h"

#include "guiutil.h"
#include "blockchainmodel.h"

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
}

InfoPage::~InfoPage()
{
    delete ui;
}

BlockchainModel * InfoPage::blockchainModel() const
{
    return model;
}
