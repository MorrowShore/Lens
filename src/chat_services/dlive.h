#pragma once

#include "chatservice.h"
#include "models/message.h"
#include "models/author.h"
#include <QWebSocket>
#include <QTimer>

class DLive : public ChatService
{
    Q_OBJECT
public:
    explicit DLive(QSettings& settings, const QString& settingsGroupPathParent, QNetworkAccessManager& network, cweqt::Manager& web, QObject *parent = nullptr);

    ConnectionStateType getConnectionStateType() const override;
    QString getStateDescription() const override;

protected:
    void reconnectImpl() override;

private slots:
    void send(const QString& type, const QJsonObject& payload = QJsonObject(), const int64_t id = -1);
    void sendStart();

    void onWebSocketReceived(const QString& text);

    void requestChatRoom(const QString& displayName);
    void requestLiveStream(const QString& displayName);

private:
    struct Info
    {
        QString userName;
        std::optional<Author::Tag> subscriberTag;
    };

    static QString extractChannelName(const QString& stream);
    static QJsonObject generateQuery(const QString& operationName, const QString& hash, const QMap<QString, QJsonValue>& variables, const QString& query = QString());

    void parseMessages(const QJsonArray& jsonMessages);
    void parseEmoji(const QJsonObject& json);
    void parseBadges(const QJsonArray& jsonBadges);

    std::shared_ptr<Author> parseAuthorFromMessage(const QJsonObject& json) const;

    QPair<std::shared_ptr<Message>, std::shared_ptr<Author>> parseChatText(const QJsonObject& json) const;
    QPair<std::shared_ptr<Message>, std::shared_ptr<Author>> parseChatGift(const QJsonObject& json) const;

    QPair<std::shared_ptr<Message>, std::shared_ptr<Author>> parseHighlighted(const QJsonObject& json, const QString& text) const;

    QNetworkAccessManager& network;
    QWebSocket socket;

    Info info;

    QTimer timerUpdaetStreamInfo;
    QTimer timerReconnect;
    QTimer checkPingTimer;

    QHash<QString, QString> emotes; // key - name, value - url
    QHash<QString, QString> badges; // key - name, value - url
};
