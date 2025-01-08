#include "transactiondescdialog.h"
#include "ui_transactiondescdialog.h"

#include "guiutil.h"
#include "transactiontablemodel.h"

#include <QLabel>
#include <QModelIndex>

TransactionDescDialog::TransactionDescDialog(const QModelIndex& idx, QWidget* parent)
    : QDialog(parent), ui(new Ui::TransactionDescDialog) {
	ui->setupUi(this);
	GUIUtil::SetBitBayFonts(this);

	QString desc = idx.data(TransactionTableModel::LongDescriptionRole).toString();
	ui->detailText->setHtml(desc);
}

TransactionDescDialog::~TransactionDescDialog() {
	delete ui;
}
