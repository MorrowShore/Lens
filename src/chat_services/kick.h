#pragma once

#include "chatservice.h"
#include "models/message.h"
#include <QTimer>
#include <QWebSocket>
#include <QJsonValue>

class Kick : public ChatService
{
    Q_OBJECT
public:
    explicit Kick(ChatHandler& manager, QSettings& settings, const QString& settingsGroupPathParent, QNetworkAccessManager& network, cweqt::Manager& web, QObject *parent = nullptr);
    
    ConnectionState getConnectionState() const override;
    QString getMainError() const override;

protected:
    void reconnectImpl() override;

private slots:
    void onWebSocketReceived(const QString &rawData);

    void send(const QJsonObject& object);
    void sendSubscribe(const QString& chatroomId);
    void sendPing();

    void requestChannelInfo(const QString& slug);

    void parseChatMessageEvent(const QJsonObject& data);
    void parseMessageDeletedEvent(const QJsonObject& data);

private:
    QString parseBadge(const QJsonObject& basgeJson, const QString& defaultValue) const;

    struct Info
    {
        QString chatroomId;
        QMap<int, QString> subscriberBadges; // <months, url>
    };

    struct User
    {
        QUrl avatar;
    };

    static QString extractChannelName(const QString& stream);
    static QList<std::shared_ptr<Message::Content>> parseContents(const QString& rawText);

    QNetworkAccessManager& network;
    cweqt::Manager& web;
    QWebSocket socket;

    Info info;

    QTimer timerPing;
    QTimer timerRequestChannelInfo;
    QTimer heartbeatAcknowledgementTimer;

    QMap<QString, User> users;
};

