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
    struct Guild
    {
        static std::optional<Guild> fromJson(const QJsonObject& object)
        {
            Guild guild;

            guild.id = object.value("id").toString().trimmed();
            guild.name = object.value("name").toString().trimmed();

            if (guild.id.isEmpty() || guild.name.isEmpty())
            {
                return std::nullopt;
            }

            return guild;
        }

        QString id;
        QString name;
    };

    struct Channel
    {
        static std::optional<Channel> fromJson(const QJsonObject& object)
        {
            Channel channel;

            channel.id = object.value("id").toString().trimmed();
            channel.name = object.value("name").toString().trimmed();
            channel.nsfw = object.value("nsfw").toBool(channel.nsfw);

            if (channel.id.isEmpty() || channel.name.isEmpty())
            {
                return std::nullopt;
            }

            return channel;
        }

        QString id;
        QString name;
        bool nsfw = false;
    };

    struct User
    {
        static User fromJson(const QJsonObject& json)
        {
            User user;

            user.avatarHash = json.value("avatar").toString();
            user.discriminator = json.value("discriminator").toString();
            user.globalName = json.value("global_name").toString();
            user.id = json.value("id").toString();
            user.username = json.value("username").toString();

            return user;
        }

        static bool isValidDiscriminator(const QString& discriminator)
        {
            return !discriminator.isEmpty() && discriminator != "0";
        }

        QString avatarHash;
        QString discriminator;
        QString globalName;
        QString id;
        QString username;

        QString getDisplayName(const bool includeDiscriminatorIfNeedAndValid) const
        {
            if (globalName.isEmpty())
            {
                QString name = username;
                if (includeDiscriminatorIfNeedAndValid && isValidDiscriminator(discriminator))
                {
                    name += "#" + discriminator;
                }

                return name;
            }

            return globalName;
        }
    };

    struct Info
    {
        int heartbeatInterval = 30000;
        QJsonValue lastSequence;
        User botUser;
        QMap<QString, Guild> guilds;
        bool guildsLoaded = false;
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

    void parseMessageCreateDefault(const QJsonObject& jsonMessage);
    void parseMessageCreateUserJoin(const QJsonObject& jsonMessage);

    void updateUI();

    void requestGuilds();
    void requestChannel(const QString& channelId);

    void processDeferredMessages(const std::optional<QString>& guildId, const std::optional<QString>& channelId);
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

    QMap<QString, Channel> channels;

    QMap<QString, QString> requestedGuildsChannels; // <guildId, channelId>
    QMap<QPair<QString, QString>, QList<QPair<std::shared_ptr<Message>, std::shared_ptr<Author>>>> deferredMessages; // QMap<<guildId, channelId>, QList<QPair<Messsage, Author>>>
};
