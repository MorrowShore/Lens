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
};

