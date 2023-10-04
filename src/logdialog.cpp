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

    setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::WindowCloseButtonHint);

    ui->messages->setModel(LogHandler::getModel());
}

LogDialog::~LogDialog()
{
    delete ui;
}
