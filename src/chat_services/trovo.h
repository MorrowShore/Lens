#pragma once

#include "chatservice.h"
#include <QWebSocket>

class Trovo : public ChatService
{
    Q_OBJECT
public:
    explicit Trovo(QSettings& settings, const QString& SettingsGroupPath, QNetworkAccessManager& network, QObject *parent = nullptr);

    ConnectionStateType getConnectionStateType() const override;
    QString getStateDescription() const override;
    void reconnect() override;

private slots:
    void onWebSocketReceived(const QString& rawData);
    void sendToWebSocket(const QJsonDocument& data);

private:
    static QString getStreamId(const QString& stream);

    QSettings& settings;
    QNetworkAccessManager& network;

    QWebSocket socket;

    QString lastConnectedChannelName;
};

