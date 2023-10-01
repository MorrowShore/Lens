#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QString>

class LogHandler : public QObject
{
    Q_OBJECT
public:
    static void initialize(const bool saveToFile);
    static QString getDirectory();
    static void openDirectory();

private:
    static void handler(QtMsgType type, const QMessageLogContext &context, const QString &text);
    static void writeToFile(QtMsgType type, const QMessageLogContext &context, const QString &text);
};
