#pragma once

#include <QString>
#include <QDateTime>
#include <QUrl>
#include <QFile>
#include <QDir>
#include <QDebug>
#include <QJsonParseError>
#include <QJsonObject>
#include <qwindowdefs.h>

#ifdef Q_OS_WINDOWS
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#include <dwmapi.h>
#endif

namespace AxelChat
{

static const QByteArray UserAgentNetworkHeaderName = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.77 Safari/537.36";

static QString simplifyUrl(const QString& url)
{
    QString withoutHttpsWWW = url;

    // \ ->/
    withoutHttpsWWW = withoutHttpsWWW.replace('\\', '/');

    //https:// http://
    if (withoutHttpsWWW.startsWith("https://"))
    {
        withoutHttpsWWW = withoutHttpsWWW.mid(8);
    }
    else if (withoutHttpsWWW.startsWith("http://"))
    {
        withoutHttpsWWW = withoutHttpsWWW.mid(7);
    }

    //www.
    if (withoutHttpsWWW.startsWith("www."))
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

    return withoutQuery;
}

static void saveDebugDataToFile(const QString& folder, const QString& fileName, const QByteArray& data)
{
    Q_UNUSED(folder)
    Q_UNUSED(fileName)
    Q_UNUSED(data)
#ifndef AXELCHAT_LIBRARY
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
#endif
}

static QByteArray convertANSIWithUtf8Numbers(const QString& string)
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

static QString removeFromStart(const QString& string, const QString& subString, const Qt::CaseSensitivity caseSensitivity, bool* ok = nullptr)
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

static QString removeFromEnd(const QString& string, const QString& subString, const Qt::CaseSensitivity caseSensitivity, bool* ok = nullptr)
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

#ifdef Q_OS_WINDOWS
static void setDarkWindowFrame(const WId wid)
{
    BOOL value = TRUE;
    ::DwmSetWindowAttribute((HWND)wid, DWMWA_USE_IMMERSIVE_DARK_MODE, &value, sizeof(value));
}
#endif

static std::unique_ptr<QJsonObject> findJsonObject(const QByteArray &data, const int startPos)
{
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data.mid(startPos), &parseError);
    if (parseError.error == QJsonParseError::GarbageAtEnd)
    {
        const int offset = parseError.offset;
        doc = QJsonDocument::fromJson(data.mid(startPos, offset), &parseError);
    }

    if (parseError.error != QJsonParseError::NoError)
    {
        qDebug() << "Warning: Json object error: " << parseError.errorString().toStdWString() << ", offset: " << parseError.offset;
    }

    std::unique_ptr<QJsonObject> result(new QJsonObject(doc.object()));
    return result;
}

static std::unique_ptr<QJsonObject> findJsonObject(const QByteArray& data, const QByteArray& objectName, int& objectPosition, int startPosition = 0)
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
        return findJsonObject(data, objectName, objectPosition, posStart + objectName.length());
    }

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
        return findJsonObject(data, objectName, objectPosition, posStart + objectName.length());
    }

    objectPosition = bracePosition;

    return findJsonObject(data, bracePosition);
}

}
