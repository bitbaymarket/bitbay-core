#ifndef INFOPAGE_H
#define INFOPAGE_H

#include <QDialog>

namespace Ui {
    class InfoPage;
}

class InfoPage : public QDialog
{
    Q_OBJECT

public:
    explicit InfoPage(QWidget *parent = 0);
    ~InfoPage();

private:
    Ui::InfoPage *ui;
};

#endif // INFOPAGE_H
