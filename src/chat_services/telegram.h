#pragma once

#include "chatservice.h"
#include <QSettings>
#include <QNetworkAccessManager>

class Telegram : public ChatService
{
    Q_OBJECT
public:
    Telegram(QSettings& settings, const QString& settingsGroupPath, QNetworkAccessManager& network, QObject *parent = nullptr);

    void reconnect() override;
    ConnectionStateType getConnectionStateType() const override;
    QString getStateDescription() const override;

private:
    QSettings& settings;
    QNetworkAccessManager& network;
};
