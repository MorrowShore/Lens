#pragma once

#include "chatservice.h"

class VkVideo : public ChatService
{
    Q_OBJECT
public:
    explicit VkVideo(QSettings& settings, const QString& settingsGroupPath, QNetworkAccessManager& network, QObject *parent = nullptr);

    ConnectionStateType getConnectionStateType() const override;
    QString getStateDescription() const override;

protected:
    void reconnectImpl() override;

private:
    QNetworkAccessManager& network;
};
