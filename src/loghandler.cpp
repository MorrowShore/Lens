#include "loghandler.h"
#include "utils.h"
#include <QDateTime>
#include <QStandardPaths>
#include <QFile>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QApplication>
#include <QTimeZone>
#include <QDesktopServices>
#include <iostream>

namespace
{

static bool saveToFile;
static QString fileName;

static QString typeToString(const QtMsgType& type)
{
    switch(type)
    {
    case QtDebugMsg:    return "D";
    case QtWarningMsg:  return "W";
    case QtCriticalMsg: return "C";
    case QtFatalMsg:    return "F";
    case QtInfoMsg:     return "I";
    }

    return "U";
}

static QJsonObject contextToJson(const QMessageLogContext& context)
{
    QJsonObject result;

    result.insert("fl", context.file);
    result.insert("ln", context.line);
    result.insert("fn", context.function);
    result.insert("ct", context.category);

    return result;
}

};

void LogHandler::initialize(const bool saveToFile_)
{
    saveToFile = saveToFile_;

    const QDateTime startTime = QDateTime::currentDateTime();

    const QString dirPath = getDirectory();
    fileName = dirPath + "/" + startTime.toString(AxelChat::DateTimeFileNameFormat) + ".log";

    const QDir dir(dirPath);
    if (!dir.exists())
    {
        if (!dir.mkpath(dirPath))
        {
            qCritical() << "failed to make path" << dirPath;
        }
    }

    qInstallMessageHandler(LogHandler::handler);

    qInfo().nospace() << QApplication::applicationDisplayName() <<
        " started, version: " << QApplication::applicationVersion() <<
        ", local time: " << startTime.toString(Qt::DateFormat::ISODateWithMs);
}

QString LogHandler::getDirectory()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/logs";
}

void LogHandler::openDirectory()
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(QString("file:///") + getDirectory()));
}

void LogHandler::handler(QtMsgType type, const QMessageLogContext &context, const QString &text)
{
    QString line = "[" + typeToString(type) + "] ";

    if (context.file != nullptr)
    {
        QString file = AxelChat::removeFromStart(context.file, "..\\src\\", Qt::CaseInsensitive);
        if (!file.contains(":/"))
        {
            file = "file:///" + file;
        }

        line += QString("%1:%2").arg(file).arg(context.line);
    }

    static const int Column0Width = 48;

    if (line.count() < Column0Width + 1)
    {
        const int needAdd = Column0Width - line.count();

        for (int i = 0; i < needAdd; ++i)
        {
            line += ' ';
        }
    }
    else
    {
        line += "\t";
    }

    line += text;

    std::cout << line.toLocal8Bit().constData() << std::endl;

    if (saveToFile)
    {
        writeToFile(type, context, text);
    }

    if (type == QtFatalMsg)
    {
        abort();
    }
}

void LogHandler::writeToFile(QtMsgType type, const QMessageLogContext &context, const QString &text)
{
    QJsonObject line;

    line.insert("tp", typeToString(type));
    line.insert("tx", text);
    line.insert("cx", contextToJson(context));

    QFile file(fileName);
    file.open(QIODevice::WriteOnly | QIODevice::Append);

    QTextStream stream(&file);
    stream << QJsonDocument(line).toJson(QJsonDocument::JsonFormat::Compact) << Qt::endl;

    file.close();
}
