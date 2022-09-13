#ifndef TWITCH_HPP
#define TWITCH_HPP

#include "utils.h"
#include "chatservice.h"
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

    ConnectionStateType getConnectionStateType() const override;
    QString getStateDescription() const override;
    void reconnect() override;

    QUrl requesGetAOuthTokenUrl() const;

signals:

public slots:

protected:
    void onParameterChangedImpl(Parameter& parameter) override;

private slots:
    void sendIRCMessage(const QString& message);
    void onIRCMessage(const QString& rawData);

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
    QNetworkAccessManager& network;

    QWebSocket socket;

    Setting<QString> oauthToken;

    QString lastConnectedChannelName;

    QTimer timerReconnect;
    QTimer timerPing;
    QTimer timerCheckPong;

    QTimer timerUpdaetStreamInfo;

    QSet<QString> usersInfoUpdated;
    QHash<QString, QString> badgesUrls;
};

#endif // TWITCH_HPP
