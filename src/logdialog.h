#pragma once

#include "models/logmodel.h"
#include <QDialog>
#include <QQmlEngine>

namespace Ui {
class LogDialog;
}

class LogDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LogDialog(QWidget *parent = nullptr);
    ~LogDialog();

    static void declareQml()
    {
        qmlRegisterUncreatableType<LogDialog>("LogDialog", 1, 0, "LogDialog", "Type cannot be created in QML");
    }

private:
    Ui::LogDialog *ui;
    LogModel* model = nullptr;
};
