#pragma once

#include "chatservice.h"
#include "models/message.h"
#include "ssemanager.h"
#include <QJsonObject>

class Rumble : public ChatService
{
    Q_OBJECT
public:
    explicit Rumble(QSettings& settings, const QString& settingsGroupPath, QNetworkAccessManager& network, QObject *parent = nullptr);

    ConnectionStateType getConnectionStateType() const override;
    QString getStateDescription() const override;

protected:
    void reconnectImpl() override;

private:
    struct Info
    {
        QString chatId;
        QString videoId;
    };

    static QString extractLinkId(const QString& rawLink);
    static QString parseChatId(const QByteArray& html);
    static QString parseVideoId(const QByteArray& html);

    void requestVideoPage();
    void requestViewers();
    void startReadChat();
    void onSseReceived(const QJsonObject& root);
    void parseMessage(const QJsonObject& user, const QJsonObject& message);
    static Message::Content* parseBlock(const QJsonObject& block);

    QNetworkAccessManager& network;
    Info info;
    SseManager sse;
};

