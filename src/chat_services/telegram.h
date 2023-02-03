#pragma once

#include "chatservice.h"
#include "models/message.h"
#include <QSettings>
#include <QNetworkAccessManager>
#include <QTimer>

class Telegram : public ChatService
{
    Q_OBJECT
public:
    Telegram(QSettings& settings, const QString& settingsGroupPath, QNetworkAccessManager& network, QObject *parent = nullptr);

    ConnectionStateType getConnectionStateType() const override;
    QString getStateDescription() const override;

protected:
    void onUiElementChangedImpl(const std::shared_ptr<UIElementBridge> element) override;
    void reconnectImpl() override;

private:
    void requestUpdates();
    void processBadChatReply();
    void parseUpdates(const QJsonArray& updates);
    void parseMessage(const QJsonObject& jsonMessage, QList<Message>& messages, QList<Author>& authors);
    void requestUserPhoto(const QString& authorId, const int64_t& userId);
    void requestPhotoFileInfo(const QString& authorId, const QString& fileId);
    void addServiceContent(QList<Message::Content*>& contents, const QString& name);

    QSettings& settings;
    QNetworkAccessManager& network;

    struct Info
    {
        int64_t botUserId = -1;
        int badChatReplies = 0;
        int64_t lastUpdateId = -1;
    };

    Info info;

    QTimer timerRequestUpdates;

    Setting<QString> botToken;
    Setting<bool> showChatTitle;
    Setting<bool> allowPrivateChat;

    QSet<int64_t> usersPhotoUpdated;
};
