#pragma once

#include <QString>
#include <QDateTime>
#include <QJsonDocument>

class QtStringUtils
{
public:
    static QString dateTimeToStringISO8601WithMsWithOffsetFromUtc(const QDateTime& dateTime);
    static QString simplifyUrl(const QString& url);
    static QByteArray convertANSIWithUtf8Numbers(const QString& string);
    static QString removeFromStart(const QString& string, const QString& subString, const Qt::CaseSensitivity caseSensitivity, bool* ok = nullptr);
    static QString removeFromEnd(const QString& string, const QString& subString, const Qt::CaseSensitivity caseSensitivity, bool* ok = nullptr);
    static std::unique_ptr<QJsonDocument> findJson(const QByteArray &data, const int startPos);
    static std::unique_ptr<QJsonDocument> findJson(const QByteArray& data, const QByteArray& objectName, const QJsonValue::Type type, int& objectPosition, int startPosition = 0);
    static QByteArray find(const QByteArray& data, const QByteArray& prefix, const QChar& postfix, const int maxLengthSearch);
    static void saveDebugDataToFile(const QString& folder, const QString& fileName, const QByteArray& data);

private:
    QtStringUtils(){}
};
