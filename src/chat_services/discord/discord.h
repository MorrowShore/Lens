#pragma once

#include "../chatservice.h"
#include "Guild.h"
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
    explicit Discord(QSettings& settings, const QString& settingsGroupPathParent, QNetworkAccessManager& network, cweqt::Manager& web, QObject *parent = nullptr);

    ConnectionStateType getConnectionStateType() const override;
    QString getStateDescription() const override;

protected:
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
        User botUser;
        bool guildsLoaded = false;

        Guild& getGuild(const QString& id)
        {
            if (guilds.contains(id))
            {
                return guilds[id];
            }

            qWarning() << Q_FUNC_INFO << "guild with id" << id << "not found";

            static Guild guild;

            return guild;
        }

        const Guild& getGuild(const QString& id) const
        {
            return const_cast<Info&>(*this).getGuild(id);
        }

        void addGuild(const Guild& guild)
        {
            if (guild.id.isEmpty())
            {
                qWarning() << Q_FUNC_INFO << "guild has empty id, name =" << guild.name;
            }

            if (guild.name.isEmpty())
            {
                qWarning() << Q_FUNC_INFO << "guild has empty name, id =" << guild.id;
            }

            guilds.insert(guild.id, guild);
        }

        const QMap<QString, Guild>& getGuilds() const
        {
            return guilds;
        }

    private:
        QMap<QString, Guild> guilds;
    };

    QNetworkRequest createRequestAsBot(const QUrl& url) const;
    bool checkReply(QNetworkReply *reply, const char *tag, QByteArray& resultData);
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

    void requestGuilds();
    void requestChannels(const QString& guildId);

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

    QTimer timerReconnect;
    QTimer heartbeatTimer;
    QTimer heartbeatAcknowledgementTimer;
};
