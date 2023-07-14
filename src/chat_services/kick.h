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
    explicit Kick(QSettings& settings, const QString& settingsGroupPathParent, QNetworkAccessManager& network, cweqt::Manager& web, QObject *parent = nullptr);

    ConnectionStateType getConnectionStateType() const override;
    QString getStateDescription() const override;

protected:
    void reconnectImpl() override;
    QString generateAuthorId(const QString& rawId) const;
    QString generateMessageId(const QString& rawId) const;

private slots:
    void onWebSocketReceived(const QString &rawData);

    void send(const QJsonObject& object);
    void sendSubscribe(const QString& chatroomId);
    void sendPing();

    void requestChannelInfo(const QString& slug);

    void parseChatMessageEvent(const QJsonObject& data);
    void parseMessageDeletedEvent(const QJsonObject& data);

private:
    struct Info
    {
        QString chatroomId;
    };

    struct User
    {
        QUrl avatar;
    };

    static QString extractChannelName(const QString& stream);
    static QList<Message::Content*> parseContents(const QString& rawText);

    QNetworkAccessManager& network;
    cweqt::Manager& web;
    QWebSocket socket;

    Info info;

    QTimer timerReconnect;
    QTimer timerPing;
    QTimer timerCheckPing;
    QTimer timerRequestChannelInfo;

    QMap<QString, User> users;
};

