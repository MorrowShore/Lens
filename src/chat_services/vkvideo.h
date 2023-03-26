#pragma once

#include "chatservice.h"
#include "oauth2.h"
#include <QTimer>

class VkVideo : public ChatService
{
    Q_OBJECT
public:
    explicit VkVideo(QSettings& settings, const QString& settingsGroupPath, QNetworkAccessManager& network, QObject *parent = nullptr);

    ConnectionStateType getConnectionStateType() const override;
    QString getStateDescription() const override;
    TcpReply processTcpRequest(const TcpRequest &request) override;

protected:
    void reconnectImpl() override;

private slots:
    void requestChat();
    void requestVideo();
    void requsetUsers(const QList<int64_t>& ids);
    void updateUI();

private:
    bool extractOwnerVideoId(const QString& videoiLink, QString& ownerId, QString& videoId);
    bool isCanConnect() const;

    struct Info
    {
        QString ownerId;
        QString videoId;
    };

    struct User
    {
        QString id;
        QString name;
        //TODO implement nickname
        bool verified = false;
        QString avatar;
    };

    Info info;

    QNetworkAccessManager& network;

    QTimer timerRequestChat;
    QTimer timerRequestVideo;

    std::shared_ptr<UIElementBridge> authStateInfo;
    std::shared_ptr<UIElementBridge> loginButton;
    OAuth2 auth;

    std::unordered_map<int64_t, User> users;
};
