#pragma once

#include "chatservice.h"
#include "models/message.h"
#include <QNetworkAccessManager>
#include <QTimer>

class Telegram : public ChatService
{
    Q_OBJECT
public:
    Telegram(QSettings& settings, const QString& settingsGroupPathParent, QNetworkAccessManager& network, cweqt::Manager& web, QObject *parent = nullptr);

    ConnectionStateType getConnectionState() const override;
    QString getStateDescription() const override;

protected:
    void reconnectImpl() override;

private:
    void requestUpdates();
    void processBadChatReply();
    void parseUpdates(const QJsonArray& updates);
    void parseMessage(const QJsonObject& jsonMessage, QList<std::shared_ptr<Message>>& messages, QList<std::shared_ptr<Author>>& authors);
    void requestUserPhoto(const QString& authorId, const int64_t& userId);
    void requestPhotoFileInfo(const QString& authorId, const QString& fileId);
    void addServiceContent(QList<std::shared_ptr<Message::Content>>& contents, const QString& name);
    void updateUI();

    QNetworkAccessManager& network;

    struct Info
    {
        QString botDisplayName;
        int64_t botUserId = -1;
        int badChatReplies = 0;
        int64_t lastUpdateId = -1;
    };

    Info info;
    
    std::shared_ptr<UIBridgeElement> authStateInfo;

    QTimer timerRequestUpdates;

    Setting<QString> botToken;
    Setting<bool> showChatTitle;
    Setting<bool> allowPrivateChat;

    QSet<int64_t> usersPhotoUpdated;
};
