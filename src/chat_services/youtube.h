#ifndef YOUTUBEINTERCEPTOR_HPP
#define YOUTUBEINTERCEPTOR_HPP

#include "models/message.h"
#include "chatservice.h"
#include <QSettings>
#include <QNetworkAccessManager>
#include <QTimer>
#include <memory>

class YouTube : public ChatService
{
    Q_OBJECT
public:
    explicit YouTube(QSettings& settings, const QString& settingsGroupPath, QNetworkAccessManager& network, QObject *parent = nullptr);

    void reconnect() override;
    ConnectionStateType getConnectionStateType() const override;
    QString getStateDescription() const override;

    Q_INVOKABLE static QUrl createResizedAvatarUrl(const QUrl& sourceAvatarUrl, int imageHeight);

private slots:
    void onTimeoutRequestChat();
    void onReplyChatPage();

    void onTimeoutRequestStreamPage();
    void onReplyStreamPage();

private:
    QString extractBroadcastId(const QString& link) const;
    void parseActionsArray(const QJsonArray& array, const QByteArray& data);
    bool parseViews(const QByteArray& rawData);
    void processBadChatReply();
    void processBadLivePageReply();

    void tryAppedToText(QList<Message::Content*>& contents, const QJsonObject& jsonObject, const QString& varName, bool bold) const;
    void parseText(const QJsonObject& message, QList<Message::Content*>& contents) const;

    QColor intToColor(quint64 rawColor) const;

    QSettings& settings;
    QNetworkAccessManager& network;

    QTimer timerRequestChat;
    QTimer timerRequestStreamPage;

    int badChatReplies = 0;
    int badLivePageReplies = 0;

    static void printData(const QString& tag, const QByteArray& data);

    const int emojiPixelSize = 24;
    const int stickerSize = 80;
    const int badgePixelSize = 64;
};

#endif // YOUTUBEINTERCEPTOR_HPP
