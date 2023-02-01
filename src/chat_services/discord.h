#pragma once

#include "chatservice.h"
#include <QTcpServer>

class Discord : public ChatService
{
    Q_OBJECT
public:
    explicit Discord(QSettings& settings, const QString& settingsGroupPath, QNetworkAccessManager& network, QObject *parent = nullptr);

    ConnectionStateType getConnectionStateType() const override;
    QString getStateDescription() const override;

protected:
    void reconnectImpl() override;

private:
    QSettings& settings;
    QNetworkAccessManager& network;

    QTcpServer server;

    Setting<QString> oauthToken;
};
