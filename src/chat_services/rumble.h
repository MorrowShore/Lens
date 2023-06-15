#pragma once

#include "chatservice.h"

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
    };

    static QString extractLinkId(const QString& rawLink);
    static QString parseChatId(const QByteArray& html);

    void requestVideoPage();
    void requestChatPage();

    QNetworkAccessManager& network;

    Info info;
};

