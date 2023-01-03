#pragma once

#include "chatservice.h"
#include <QTimer>

class VkPlayLive : public ChatService
{
public:
    explicit VkPlayLive(QSettings& settings, const QString& settingsGroupPath, QNetworkAccessManager& network, QObject *parent = nullptr);

    ConnectionStateType getConnectionStateType() const override;
    QString getStateDescription() const override;
    void reconnect() override;

private slots:
    void onTimeoutRequestChat();
    void onReplyChat();

private:
    static QString extractChannelName(const QString& stream);

    QSettings& settings;
    QNetworkAccessManager& network;

    QTimer timerRequestChat;
};
