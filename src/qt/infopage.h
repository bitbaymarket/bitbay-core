#ifndef INFOPAGE_H
#define INFOPAGE_H

#include <QDialog>

namespace Ui {
    class InfoPage;
}

class BlockchainModel;

class InfoPage : public QDialog
{
    Q_OBJECT

public:
    explicit InfoPage(QWidget *parent = 0);
    ~InfoPage();

    BlockchainModel * blockchainModel() const;
    
private:
    Ui::InfoPage *ui;
    BlockchainModel *model;
};

#endif // INFOPAGE_H
