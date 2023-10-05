#pragma once

#include <QObject>
#include <QString>
#include <QVariant>
#include <QDateTime>

class LogMessage
{
    Q_GADGET
public:
    enum class Type {
        Other,
        Debug,
        Warning,
        Critical,
        Fatal,
        Info,
    };
    Q_ENUM(Type)

    static Type fromQtType(const QtMsgType type)
    {
        switch(type)
        {
        case QtDebugMsg:    return Type::Debug;
        case QtWarningMsg:  return Type::Warning;
        case QtCriticalMsg: return Type::Critical;
        case QtFatalMsg:    return Type::Fatal;
        case QtInfoMsg:     return Type::Info;
        }

        return Type::Other;
    }

    LogMessage(){}
    LogMessage(const QString& text_, const Type type_ = Type::Other, const QString& file_ = QString(), const QString& function_ = QString(), int line_ = -1)
        : text(text_)
        , type(type_)
        , file(file_)
        , function(function_)
        , line(line_)
    {

    }

    QString text;
    Type type = Type::Other;
    QString file;
    QString function;
    int line = -1;
    QDateTime time;
};
