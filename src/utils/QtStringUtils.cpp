#include "QtStringUtils.h"
#include <QTimeZone>
#include <QDebug>
#include <QFile>
#include <QDir>

QString QtStringUtils::dateTimeToStringISO8601WithMsWithOffsetFromUtc(const QDateTime &dateTime)
{
    QString result = dateTime.toUTC().toString(Qt::DateFormat::ISODateWithMs);
    if (result.endsWith('Z', Qt::CaseSensitivity::CaseInsensitive))
    {
        result = result.left(result.length() - 1);
    }

    const int offsetMs = dateTime.offsetFromUtc();
    const bool belowZero = offsetMs < 0;

    const QTime offset = QTime::fromMSecsSinceStartOfDay(abs(offsetMs) * 1000);
    const QString offsetStr = QString("%1:%2").arg(offset.hour(), 2, 10, QChar('0')).arg(offset.minute(), 2, 10, QChar('0'));

    if (belowZero)
    {
        result += "-";
    }
    else
    {
        result += "+";
    }

    result += offsetStr;

    return result;
}

QString QtStringUtils::simplifyUrl(const QString &url)
{
    QString withoutHttpsWWW = url;

    // \ ->/
    withoutHttpsWWW = withoutHttpsWWW.replace('\\', '/');

    //https:// http://
    if (withoutHttpsWWW.startsWith("https://", Qt::CaseSensitivity::CaseInsensitive))
    {
        withoutHttpsWWW = withoutHttpsWWW.mid(8);
    }
    else if (withoutHttpsWWW.startsWith("http://", Qt::CaseSensitivity::CaseInsensitive))
    {
        withoutHttpsWWW = withoutHttpsWWW.mid(7);
    }

    //www.
    if (withoutHttpsWWW.startsWith("www.", Qt::CaseSensitivity::CaseInsensitive))
    {
        withoutHttpsWWW = withoutHttpsWWW.mid(4);
    }

    // remove last '/'
    if (withoutHttpsWWW.endsWith("/"))
    {
        withoutHttpsWWW = withoutHttpsWWW.left(withoutHttpsWWW.lastIndexOf('/'));
    }

    //?
    QString withoutQuery = withoutHttpsWWW;
    if (withoutHttpsWWW.contains('?'))
    {
        withoutQuery = withoutHttpsWWW.left(withoutHttpsWWW.indexOf('?'));
    }

    //#
    QString withoutSign = withoutQuery;
    if (withoutQuery.contains('#'))
    {
        withoutSign = withoutSign.left(withoutSign.indexOf('#'));
    }

    return withoutSign;
}

QByteArray QtStringUtils::convertANSIWithUtf8Numbers(const QString &string)
{
    const QVector<uint>& ucs4str = string.toUcs4();
    QByteArray ba;
    ba.reserve(ucs4str.count() * 4);

    for (const uint& c : ucs4str)
    {
        if (c >= 32u && c <= 126u && c!= 35u && c!= 60u && c!= 62u)
        {
            ba += (char)c;
        }
        else
        {
            ba += "<" + QByteArray::number(c) + ">";
        }
    }

    return ba;
}

QString QtStringUtils::removeFromStart(const QString &string, const QString &subString, const Qt::CaseSensitivity caseSensitivity, bool *ok)
{
    if (!string.startsWith(subString, caseSensitivity))
    {
        if (ok)
        {
            *ok = false;
        }

        return string;
    }

    if (ok)
    {
        *ok = true;
    }

    return string.mid(subString.length());
}

QString QtStringUtils::removeFromEnd(const QString &string, const QString &subString, const Qt::CaseSensitivity caseSensitivity, bool *ok)
{
    if (!string.endsWith(subString, caseSensitivity))
    {
        if (ok)
        {
            *ok = false;
        }

        return string;
    }

    if (ok)
    {
        *ok = true;
    }

    return string.left(string.lastIndexOf(subString, -1, caseSensitivity));
}

std::unique_ptr<QJsonDocument> QtStringUtils::findJson(const QByteArray &data, const int startPos)
{
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data.mid(startPos), &parseError);
    if (parseError.error == QJsonParseError::GarbageAtEnd)
    {
        const int offset = parseError.offset;
        doc = QJsonDocument::fromJson(data.mid(startPos, offset), &parseError);
    }
    else if (parseError.error != QJsonParseError::NoError && parseError.error != QJsonParseError::UnterminatedObject)
    {
        qDebug() << "Warning: Json object error: " << parseError.errorString().toStdWString() << ", offset: " << parseError.offset;
    }

    return std::unique_ptr<QJsonDocument>(new QJsonDocument(doc));
}

std::unique_ptr<QJsonDocument> QtStringUtils::findJson(const QByteArray &data, const QByteArray &objectName, const QJsonValue::Type type, int &objectPosition, int startPosition)
{
    objectPosition = -1;

    // find object name

    const int posStart = data.indexOf("\"" + objectName + "\"", startPosition);
    if (posStart == -1)
    {
        return nullptr;
    }

    // find colon

    static const int FindSymbolLength = 20;
    const int posAfterObjectName = posStart + objectName.length() + 2;
    int colonPosition = -1;
    for (int pos = posAfterObjectName; pos < posAfterObjectName + FindSymbolLength; ++pos)
    {
        if (data[pos] == ':')
        {
            colonPosition = pos;
            break;
        }
    }

    if (colonPosition == -1)
    {
        return findJson(data, objectName, type, objectPosition, posStart + objectName.length());
    }

    if (type == QJsonValue::Type::Object)
    {
        // find brace

        int bracePosition = -1;
        for (int pos = colonPosition + 1; pos < posAfterObjectName + FindSymbolLength; ++pos)
        {
            if (data[pos] == '{')
            {
                bracePosition = pos;
                break;
            }
        }

        if (bracePosition == -1)
        {
            return findJson(data, objectName, type, objectPosition, posStart + objectName.length());
        }

        objectPosition = bracePosition;
    }
    else if (type == QJsonValue::Type::Array)
    {
        // find [

        int squarePosition = -1;
        for (int pos = colonPosition + 1; pos < posAfterObjectName + FindSymbolLength; ++pos)
        {
            if (data[pos] == '[')
            {
                squarePosition = pos;
                break;
            }
        }

        if (squarePosition == -1)
        {
            return findJson(data, objectName, type, objectPosition, posStart + objectName.length());
        }

        objectPosition = squarePosition;
    }
    else
    {
        qCritical() << "unksupported json value type";
    }

    return findJson(data, objectPosition);
}

QByteArray QtStringUtils::find(const QByteArray &data, const QByteArray &prefix, const QChar &postfix, const int maxLengthSearch)
{
    const int prefixStartPos = data.indexOf(prefix);
    if (prefixStartPos == -1)
    {
        //qWarning() << "failed to find prefix";
        return QByteArray();
    }

    const int resultStartPos = prefixStartPos + prefix.length();

    const int lastSearchPos = (maxLengthSearch > 0) ? std::min(data.length(), resultStartPos + maxLengthSearch) : data.length();

    int resultLastPos = -1;
    for (int i = resultStartPos; i < lastSearchPos; ++i)
    {
        const QChar& c = data[i];
        if (c == postfix)
        {
            resultLastPos = i;
            break;
        }
    }

    if (resultLastPos == -1)
    {
        //qDebug() << "not found '\"'";
        return QByteArray();
    }

    return data.mid(resultStartPos, resultLastPos - resultStartPos);
}

void QtStringUtils::saveDebugDataToFile(const QString &folder, const QString &fileName, const QByteArray &data)
{
    Q_UNUSED(folder)
    Q_UNUSED(fileName)
    Q_UNUSED(data)
#ifndef QT_NO_DEBUG
    QDir().mkpath(folder);

    QFile file(folder + "/" + fileName);
    if (file.open(QFile::OpenModeFlag::WriteOnly | QFile::OpenModeFlag::Text))
    {
        file.write(data);
        file.close();
        qDebug() << "Saved to" << file.fileName();
    }
    else
    {
        qDebug() << "Failed to save" << file.fileName();
    }
#endif
}

void QtStringUtils::toUpperFirstChar(QString &text)
{
    if (text.isEmpty())
    {
        return;
    }

    text[0] = text[0].toUpper();
}
