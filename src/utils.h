#pragma once

#include <QString>
#include <QDateTime>
#include <QUrl>
#include <QFile>
#include <QDir>
#include <QDebug>

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

static bool removeFromStart(QString& string, const QString& subString, const Qt::CaseSensitivity caseSensitivity)
{
    if (!string.startsWith(subString, caseSensitivity))
    {
        return false;
    }

    string = string.mid(subString.length());
    return true;
}

static bool removeFromEnd(QString& string, const QString& subString, const Qt::CaseSensitivity caseSensitivity)
{
    if (!string.endsWith(subString, caseSensitivity))
    {
        return false;
    }

    string = string.left(string.lastIndexOf(subString, -1, caseSensitivity));
    return true;
}

}
