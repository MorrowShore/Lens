#ifndef TWITCH_HPP
#define TWITCH_HPP

#include "utils.hpp"
#include "chatservice.hpp"
#include <QSettings>
#include <QWebSocket>
#include <QTimer>
#include <QNetworkAccessManager>

class Twitch : public ChatService
{
    Q_OBJECT
    Q_PROPERTY(QUrl     requesGetAOuthTokenUrl      READ requesGetAOuthTokenUrl     CONSTANT)

public:
    explicit Twitch(QSettings& settings, const QString& settingsGroupPath, QNetworkAccessManager& network, QObject *parent = nullptr);
    ~Twitch();
    ConnectionStateType getConnectionStateType() const override;
    QString getStateDescription() const override;
    QUrl requesGetAOuthTokenUrl() const;
    void setBroadcastLink(const QString &link) override;
    QString getBroadcastLink() const override;

    void reconnect() override;

signals:

public slots:

protected:
    void onParameterChanged(Parameter& parameter) override;

private slots:
    void sendIRCMessage(const QString& message);
    void onIRCMessage(const QString& rawData);

    void requestForAvatarsByChannelPage(const QString& channelLogin);
    void onReplyAvatarsByChannelPage();

    void requestForGlobalBadges();
    void requestForChannelBadges(const QString& broadcasterId);
    void onReplyBadges();

    void requestUserInfo(const QString& login);
    void onReplyUserInfo();

    void requestStreamInfo(const QString& login);
    void onReplyStreamInfo();

private:
    struct Info
    {
        QString broadcasterId;
        QString userSpecifiedChannel;
    };

    void parseBadgesJson(const QByteArray& data);

    struct MessageEmoteInfo
    {
        QString id;
        QList<QPair<int, int>> indexes;

        bool isValid() const
        {
            return !id.isEmpty() && !indexes.isEmpty();
        }
    };

    QSettings& settings;
    const QString SettingsGroupPath;
    QNetworkAccessManager& network;

    QWebSocket _socket;

    Setting<QString> oauthToken;

    Info _info;
    QString _lastConnectedChannelName;

    QTimer _timerReconnect;
    QTimer _timerPing;
    QTimer _timerCheckPong;

    QTimer _timerUpdaetStreamInfo;

    QHash<QNetworkReply*, QString> repliesForAvatar; // <reply, channel_id>
    QHash<QString, QUrl> avatarsUrls; // <channel_name, avatar_url>
    QHash<QString, QString> _badgesUrls;
};

#endif // TWITCH_HPP
