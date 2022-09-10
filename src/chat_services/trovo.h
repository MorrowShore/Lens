#pragma once

#include "chatservice.h"

class Trovo : public ChatService
{
    Q_OBJECT
public:
    explicit Trovo(QSettings& settings, const QString& SettingsGroupPath, QNetworkAccessManager& network, QObject *parent = nullptr);

    ConnectionStateType getConnectionStateType() const override;
    QString getStateDescription() const override;
    void reconnect() override;

};

