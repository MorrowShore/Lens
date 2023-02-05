#pragma once

#include "chatservice.h"
#include "models/message.h"
#include "models/author.h"
#include <QWebSocket>
#include <QNetworkReply>
#include <QTimer>
#include <QNetworkRequest>

class Discord : public ChatService
{
    Q_OBJECT
public:
    explicit Discord(QSettings& settings, const QString& settingsGroupPath, QNetworkAccessManager& network, QObject *parent = nullptr);

    ConnectionStateType getConnectionStateType() const override;
    QString getStateDescription() const override;

protected:
    void onUiElementChangedImpl(const std::shared_ptr<UIElementBridge> element) override;
    void reconnectImpl() override;

private slots:
    void onWebSocketReceived(const QString& rawData);
    void sendHeartbeat();
    void sendIdentify();

private:
    struct Info
    {
        int heartbeatInterval = 30000;
        QJsonValue lastSequence;
        QString userName;
        QString userDiscriminator;
    };

    struct Guild
    {
        QString id;
        QString name;
    };

    struct Channel
    {
        QString id;
        QString name;
        bool nsfw = false;
    };

    QNetworkRequest createRequestAsBot(const QUrl& url) const;
    bool checkReply(QNetworkReply *reply, const char *tag, QByteArray& resultData);
    bool isCanConnect() const;
    void processDisconnected();
    void processConnected();

    void send(const int opCode, const QJsonValue& data);

    void parseDispatch(const QString& eventType, const QJsonObject& data);
    void parseHello(const QJsonObject& data);
    void parseInvalidSession(const bool resumableSession);
    void parseMessageCreate(const QJsonObject& jsonMessage);

    void updateUI();

    void requestGuild(const QString& guildId);
    void requestChannel(const QString& channelId);

    void processDeferredMessages(const std::optional<QString>& guildId, const std::optional<QString>& channelId);
    QString getDestination(const Guild& guild, const Channel& channel) const;
    bool isValidForShow(const Message& message, const Author& author, const Guild& guild, const Channel& channel) const;

    QSettings& settings;
    QNetworkAccessManager& network;

    Info info;

    Setting<QString> applicationId;
    Setting<QString> botToken;
    std::shared_ptr<UIElementBridge> authStateInfo;
    std::shared_ptr<UIElementBridge> connectBotToGuild;
    Setting<bool> showNsfwChannels;
    Setting<bool> showGuildName;
    Setting<bool> showChannelName;

    QWebSocket socket;

    QTimer timerReconnect;
    QTimer heartbeatTimer;
    QTimer heartbeatAcknowledgementTimer;

    QMap<QString, Guild> guilds;
    QMap<QString, Channel> channels;

    QMap<QString, QString> requestedGuildsChannels; // <guildId, channelId>
    QMap<QPair<QString, QString>, QList<QPair<Message, Author>>> deferredMessages; // QMap<<guildId, channelId>, QList<QPair<Messsage, Author>>>
};
