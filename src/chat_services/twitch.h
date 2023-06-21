#ifndef TWITCH_HPP
#define TWITCH_HPP

#include "utils.h"
#include "chatservice.h"
#include "oauth2.h"
#include <QWebSocket>
#include <QTimer>
#include <QNetworkAccessManager>

class Twitch : public ChatService
{
    Q_OBJECT

public:
    explicit Twitch(QSettings& settings, const QString& settingsGroupPathParent, QNetworkAccessManager& network, QObject *parent = nullptr);

    ConnectionStateType getConnectionStateType() const override;
    QString getStateDescription() const override;
    TcpReply processTcpRequest(const TcpRequest &request) override;

signals:

public slots:

protected:
    void onUiElementChangedImpl(const std::shared_ptr<UIElementBridge> element) override;
    void reconnectImpl() override;

private slots:
    void sendIRCMessage(const QString& message);
    void onIRCMessage(const QString& rawData);

    void requestGlobalBadges();
    void requestChannelBadges();
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
        QString broadcasterId;
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

    std::shared_ptr<UIElementBridge> authStateInfo;
    std::shared_ptr<UIElementBridge> loginButton;

    QTimer timerReconnect;
    QTimer timerPing;
    QTimer timerCheckPong;
    QTimer timerUpdaetStreamInfo;

    Info info;
    QHash<QString, QString> badgesUrls;

    OAuth2 auth;
};

#endif // TWITCH_HPP
