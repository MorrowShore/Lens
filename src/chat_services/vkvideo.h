#pragma once

#include "chatservice.h"
#include "oauth2.h"
#include <QTimer>

class VkVideo : public ChatService
{
    Q_OBJECT
public:
    explicit VkVideo(QSettings& settings, const QString& settingsGroupPathParent, QNetworkAccessManager& network, cweqt::Manager& web, QObject *parent = nullptr);
    
    ConnectionState getConnectionState() const override;
    QString getMainError() const override;
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
    static QUrl parseSticker(const QJsonObject& jsonSticker);
    bool checkReply(QNetworkReply *reply, const char *tag, QByteArray& resultData);
    QPair<std::shared_ptr<Message>, std::shared_ptr<Author>> parseMessage(const QJsonObject& json);

    struct Info
    {
        QString ownerId;
        QString videoId;
        int64_t startOffset = -1;
        int64_t lastMessageId = -1;

        bool hasChat = false;
    };

    struct User
    {
        QString id;
        QString name;
        bool verified = false;
        QString avatar;
        bool isGroup = false;
        QString groupType;
    };

    Info info;

    QNetworkAccessManager& network;

    QTimer timerRequestChat;
    QTimer timerRequestVideo;
    
    std::shared_ptr<UIBridgeElement> authStateInfo;
    std::shared_ptr<UIBridgeElement> loginButton;
    OAuth2 auth;

    std::unordered_map<int64_t, User> users;
};
