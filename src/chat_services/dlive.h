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

    void parseMessages(const QJsonArray& jsonMessages);

    void parseEmoji(const QJsonObject& json);

private:
    struct Info
    {
        QString userName;
    };

    static QString extractChannelName(const QString& stream);
    static QJsonObject generateQuery(const QString& operationName, const QString& hash, const QMap<QString, QJsonValue>& variables, const QString& query = QString());

    std::shared_ptr<Author> parseSender(const QJsonObject& json) const;
    QPair<std::shared_ptr<Message>, std::shared_ptr<Author>> parseChatText(const QJsonObject& json) const;
    QPair<std::shared_ptr<Message>, std::shared_ptr<Author>> parseChatGift(const QJsonObject& json) const;
    QPair<std::shared_ptr<Message>, std::shared_ptr<Author>> parseChatFollow(const QJsonObject& json) const;
    QPair<std::shared_ptr<Message>, std::shared_ptr<Author>> parseChatTCValueAdd(const QJsonObject& json) const;

    QNetworkAccessManager& network;
    QWebSocket socket;

    Info info;

    QTimer timerUpdaetStreamInfo;
    QTimer timerReconnect;
    QTimer checkPingTimer;

    QHash<QString, QString> emotes; // key - name, value - url
};
