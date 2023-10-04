#include "logdialog.h"
#include "ui_logdialog.h"
#include "loghandler.h"
#include "utils.h"

LogDialog::LogDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::LogDialog)
{
    ui->setupUi(this);

#ifdef Q_OS_WINDOWS
    AxelChat::setDarkWindowFrame(winId());
#endif

    setWindowFlags(Qt::Window);

    ui->messages->setModel(LogHandler::getModel());

    ui->messages->setColumnWidth(0, 1200);
    ui->messages->setColumnWidth(1, 300);
    ui->messages->setColumnWidth(2, 400);
}

LogDialog::~LogDialog()
{
    delete ui;
}
