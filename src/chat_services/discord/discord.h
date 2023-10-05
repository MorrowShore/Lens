#pragma once

#include "../chatservice.h"
#include "guildsstorage.h"
#include "User.h"
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
    static const QString ApiPrefix;

    explicit Discord(QSettings& settings, const QString& settingsGroupPathParent, QNetworkAccessManager& network, cweqt::Manager& web, QObject *parent = nullptr);
    
    ConnectionState getConnectionState() const override;
    QString getStateDescription() const override;

    QNetworkRequest createRequestAsBot(const QUrl& url) const;
    bool checkReply(QNetworkReply *reply, const char *tag, QByteArray& resultData);

protected:
    void reconnectImpl() override;

private slots:
    void onWebSocketReceived(const QString& rawData);
    void sendHeartbeat();
    void sendIdentify();

private:

    struct Info
    {
        Info(Discord& discord, QNetworkAccessManager& network)
            : guilds(std::make_unique<GuildsStorage>(discord, network))
        {
        }

        int heartbeatInterval = 30000;
        QJsonValue lastSequence;
        User botUser;
        std::unique_ptr<GuildsStorage> guilds;
    };

    bool isCanConnect() const;
    void processDisconnected();
    void tryProcessConnected();

    void send(const int opCode, const QJsonValue& data);

    void parseDispatch(const QString& eventType, const QJsonObject& data);
    void parseHello(const QJsonObject& data);
    void parseInvalidSession(const bool resumableSession);
    void parseMessageCreate(const QJsonObject& jsonMessage);

    void parseMessageCreateDefault(const QJsonObject& jsonMessage);
    void parseMessageCreateUserJoin(const QJsonObject& jsonMessage);

    void updateUI();

    QStringList getDestination(const Guild& guild, const Channel& channel) const;
    bool isValidForShow(const Message& message, const Author& author, const Guild& guild, const Channel& channel) const;

    static QString getEmbedTypeName(const QString& type);
    static QList<std::shared_ptr<Message::Content>> parseEmbed(const QJsonObject& jsonEmbed);
    static QList<std::shared_ptr<Message::Content>> parseAttachment(const QJsonObject& jsonAttachment);

    QNetworkAccessManager& network;

    Info info;

    Setting<QString> applicationId;
    Setting<QString> botToken;
    std::shared_ptr<UIBridgeElement> authStateInfo;
    std::shared_ptr<UIBridgeElement> connectBotToGuild;
    Setting<bool> showNsfwChannels;
    Setting<bool> showGuildName;
    Setting<bool> showChannelName;
    Setting<bool> showJoinsToServer;

    QWebSocket socket;

    QTimer heartbeatTimer;
    QTimer heartbeatAcknowledgementTimer;
};
