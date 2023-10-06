#pragma once

#include "chatservice.h"
#include "models/message.h"
#include <QTimer>
#include <QWebSocket>
#include <QJsonValue>

class Wasd : public ChatService
{
    Q_OBJECT
public:
    enum class SocketIO2Type
    {
        CONNECT = 0,
        DISCONNECT = 1,
        EVENT = 2,
        ACK = 3,
        CONNECT_ERROR = 4,
        BINARY_EVENT = 5,
        BINARY_ACK = 6,
    };
    
    explicit Wasd(ChatManager& manager, QSettings& settings, const QString& settingsGroupPathParent, QNetworkAccessManager& network, cweqt::Manager& web, QObject *parent = nullptr);
    
    ConnectionState getConnectionState() const override;
    QString getMainError() const override;

protected:
    void reconnectImpl() override;

private slots:
    void onWebSocketReceived(const QString &rawData);
    void requestTokenJWT();
    void requestChannel(const QString& channelName);
    void requestSmiles();

private:
    static QString extractChannelName(const QString& stream);

    void send(const SocketIO2Type type, const QByteArray& id = QByteArray(), const QJsonDocument& payload = QJsonDocument());
    void sendJoin(const QString& streamId, const QString& channelId, const QString& jwt);
    void sendPing();
    void parseSocketMessage(const SocketIO2Type type, const QJsonDocument& doc);
    void parseEvent(const QString& type, const QJsonObject& data);
    void parseEventMessage(const QJsonObject& data);
    void parseEventJoined(const QJsonObject& data);
    void parseSmile(const QJsonObject& jsonSmile);
    QList<std::shared_ptr<Message::Content>> parseText(const QString& text) const;

    struct Info
    {
        QString jwtToken;
        QString channelId;
        QString streamId;
    };

    Info info;

    QMap<QString, QUrl> smiles;

    QNetworkAccessManager& network;
    QWebSocket socket;

    QTimer timerReconnect;
    QTimer timerPing;
    QTimer timerRequestChannel;
};
