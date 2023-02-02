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
    bool isAuthorized() const;
    void updateAuthState();

    QSettings& settings;
    QNetworkAccessManager& network;

    QTcpServer authServer;

    Parameter authStateInfo;

    Setting<QString> oauthToken;
    Setting<QString> channel;
};
