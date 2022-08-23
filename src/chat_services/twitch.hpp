#ifndef TWITCH_HPP
#define TWITCH_HPP

#include "types.hpp"
#include "chatservice.hpp"
#include <QSettings>
#include <QWebSocket>
#include <QTimer>
#include <QNetworkAccessManager>

class Twitch : public ChatService
{
    Q_OBJECT
    Q_PROPERTY(QUrl     requesGetAOuthTokenUrl      READ requesGetAOuthTokenUrl     CONSTANT)
    Q_PROPERTY(QString  oauthToken                  READ oauthToken                 WRITE setOAuthToken                 NOTIFY stateChanged)

public:
    explicit Twitch(QSettings& settings, const QString& settingsGroupPath, QNetworkAccessManager& network, QObject *parent = nullptr);
    ~Twitch();
    ConnectionStateType connectionStateType() const override;
    QString stateDescription() const override;
    int viewersCount() const override;
    QUrl requesGetAOuthTokenUrl() const;
    QUrl chatUrl() const override;
    QUrl controlPanelUrl() const override;
    QUrl broadcastUrl() const override;
    void setBroadcastLink(const QString &link) override;
    QString getBroadcastLink() const override;

    QString oauthToken() const { return _info.oauthToken; }

    AxelChat::TwitchInfo getInfo() const;

    void reconnect() override;

signals:

public slots:
    void setOAuthToken(QString token);

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

    AxelChat::TwitchInfo _info;
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
