#pragma once

#include <QString>

class LogHandler
{
public:
    static void initialize();

private:
    static void handler(QtMsgType type, const QMessageLogContext &context, const QString &text);
    static void writeToFile(QtMsgType type, const QMessageLogContext &context, const QString &text);
};
