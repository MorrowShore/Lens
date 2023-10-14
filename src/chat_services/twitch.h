#pragma once

#include "utils/QtAxelChatUtils.h"
#include "chatservice.h"
#include "oauth2.h"
#include <QWebSocket>
#include <QTimer>
#include <QNetworkAccessManager>

class Twitch : public ChatService
{
    Q_OBJECT

public:

    struct ChannelInfo
    {
        QString id;
        QString login;
    };
    
    explicit Twitch(ChatManager& manager, QSettings& settings, const QString& settingsGroupPathParent, QNetworkAccessManager& network, cweqt::Manager& web, QObject *parent = nullptr);
    
    ConnectionState getConnectionState() const override;
    QString getMainError() const override;
    TcpReply processTcpRequest(const TcpRequest &request) override;
    QStringList getWarnings() const override;

signals:
    void authorized(const ChannelInfo& channelInfo);
    void connectedChannel(const ChannelInfo& channelInfo);

public slots:

protected:
    void reconnectImpl() override;

private slots:
    void sendIRCMessage(const QString& message);
    void onIRCMessage(const QString& rawData);

    void requestGlobalBadges();
    void requestChannelBadges(const ChannelInfo& channelInfo);
    void onReplyBadges();

    void requestUserInfo(const QString& login);
    void onReplyUserInfo();

    void requestStreamInfo(const QString& login);
    void onReplyStreamInfo();

    void onAuthStateChanged();

private:
    bool checkReply(QNetworkReply *reply, const char *tag, QByteArray& resultData);
    void parseBadgesJson(const QByteArray& data);

    struct Info
    {
        ChannelInfo connectedChannel;
        QSet<QString> usersInfoUpdated;
    };

    struct MessageEmoteInfo
    {
        QString id;
        QList<QPair<int, int>> indexes;

        bool isValid() const
        {
            return !id.isEmpty() && !indexes.isEmpty();
        }
    };

    QNetworkAccessManager& network;

    QWebSocket socket;
    
    std::shared_ptr<UIBridgeElement> authStateInfo;
    std::shared_ptr<UIBridgeElement> tokenLineEdit;
    Setting<QString> customToken;

    QList<std::shared_ptr<UIBridgeElement>> notLoggedInElements;
    QList<std::shared_ptr<UIBridgeElement>> loggedInElements;

    QTimer timerPing;
    QTimer timerCheckPong;
    QTimer timerUpdaetStreamInfo;

    Info info;
    ChannelInfo authorizedChannel;
    QHash<QString, QString> badgesUrls;

    OAuth2 auth;
};
