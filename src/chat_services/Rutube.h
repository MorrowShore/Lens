#pragma once

#include "chatservice.h"
#include <QNetworkAccessManager>
#include <QTimer>

class Rutube : public ChatService
{
    Q_OBJECT
public:
    explicit Rutube(ChatManager& manager, QSettings& settings, const QString& settingsGroupPathParent, QNetworkAccessManager& network, cweqt::Manager& web, QObject *parent = nullptr);

    ConnectionState getConnectionState() const override;
    QString getMainError() const override;

protected:
    void reconnectImpl() override;

private:
    struct Info
    {
        int badChatPageReplies = 0;
    };

    QNetworkAccessManager &network;

    Info info;
};
