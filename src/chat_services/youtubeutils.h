#pragma once

#include "models/author.h"
#include "models/message.h"
#include <QString>
#include <QJsonArray>

class YouTubeUtils
{
public:
    static const QString FolderLogs;

    YouTubeUtils() = delete;

    static QString extractBroadcastId(const QString& link);
    static void parseActionsArray(const QJsonArray& array,
                                  const QByteArray& data,
                                  QList<std::shared_ptr<Message>>& messages,
                                  QList<std::shared_ptr<Author>>& authors);

    static void printData(const QString& tag, const QByteArray& data);

    static QUrl createResizedAvatarUrl(const QUrl& sourceAvatarUrl, int imageHeight);

private:
    static void tryAppedToText(QList<std::shared_ptr<Message::Content>>& contents, const QJsonObject& jsonObject, const QString& varName, bool bold);
    static void parseText(const QJsonObject& message, QList<std::shared_ptr<Message::Content>>& contents);

};
