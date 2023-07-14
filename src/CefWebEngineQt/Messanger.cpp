#include "Messanger.h"
#include <QDebug>

namespace
{

void removeEndline(QByteArray& line)
{
    if (line.endsWith("\n"))
    {
        line.remove(line.length() - 1, 1);

        if (line.endsWith("\r"))
        {
            line.remove(line.length() - 1, 1);
        }
    }
    else if (line.endsWith("\r"))
    {
        line.remove(line.length() - 1, 1);

        if (line.endsWith("\n"))
        {
            line.remove(line.length() - 1, 1);
        }
    }
}

}

namespace cweqt
{

Messanger::Messanger(QObject *parent)
    : QObject{parent}
{

}

int64_t Messanger::send(const Message &rawMessage, QProcess *process)
{
    if (!process)
    {
        qWarning() << Q_FUNC_INFO << "process is null";
        return -1;
    }

    prevMessageId++;

    QByteArray message = rawMessage.type.toUtf8() + " " + QByteArray::number(rawMessage.data.size()) + " " + QByteArray::number(prevMessageId) + "\n";

    const QList<QString> names = rawMessage.parameters.keys();
    for (const QString& name : qAsConst(names))
    {
        message += name.toUtf8() + ":" + rawMessage.parameters[name].toUtf8() + "\n";
    }

    message += "\n" + rawMessage.data + "\n";

    process->write(message);

    return prevMessageId;
}

void Messanger::parseLine(const QByteArray &line_)
{
    QByteArray line = line_;
    removeEndline(line);

    if (message.part == Message::Part::Title)
    {
        message = Message();

        QList<QByteArray> titleParts = line.split(' ');
        if (titleParts.count() < 3)
        {
            qWarning() << Q_FUNC_INFO << "Wrong message: not found message size or id";
            return;
        }

        const QString type = titleParts[0].trimmed();
        if (type.isEmpty())
        {
            qWarning() << Q_FUNC_INFO << "Wrong message: name is empty";
            return;
        }

        bool ok = false;
        const int64_t size = titleParts[1].trimmed().toLongLong(&ok);
        if (!ok)
        {
            qWarning() << Q_FUNC_INFO << "Wrong message: failed to convert size";
            return;
        }

        const int64_t id = titleParts[2].trimmed().toLongLong(&ok);
        if (!ok)
        {
            qWarning() << Q_FUNC_INFO << "Wrong message: failed to convert message id";
            return;
        }

        message.size = size;
        message.id = id;
        message.type = type;

        message.part = Message::Part::Parameters;
    }
    else if (message.part == Message::Part::Parameters)
    {
        const int separatorPos = line.indexOf(':');
        if (separatorPos < 0)
        {
            if (line.trimmed().isEmpty())
            {
                message.part = Message::Part::Data;
                return;
            }
            else
            {
                qWarning() << Q_FUNC_INFO << "Wrong message: failed to find parameter separator";
                return;
            }
        }

        const QString name = line.left(separatorPos).trimmed();
        if (name.isEmpty())
        {
            qWarning() << Q_FUNC_INFO << "Wrong message: parameter name is empty";
            return;
        }

        QString value = line.mid(separatorPos + 1).trimmed();
        if (value.size() >= 2 && (
                (value[0] == L'\"' && value[value.size() - 1] == L'\"') ||
                (value[0] == L'\'' && value[value.size() - 1] == L'\'')))
        {
            value = value.mid(1, value.length() - 3);
        }

        message.parameters.insert(name, value);
    }
    else if (message.part == Message::Part::Data)
    {
        message.data += line;
        if (message.data.length() >= message.size)
        {
            message.data = message.data.left(message.size);

            emit messageReceived(message);

            message = Message();
        }
    }
}

QString Messanger::Message::getRawHeader() const
{
    QString text = QString("%1 %2 %3").arg(type).arg(size).arg(id);

    const QStringList keys = parameters.keys();
    for (const QString& key : qAsConst(keys))
    {
        const QString value = parameters[key];

        text += "\n" + key + ": ";

        if (value.contains(' '))
        {
            text += "\"" + value + "\"";
        }
        else
        {
            text += value;
        }
    }

    return text;
}

}
