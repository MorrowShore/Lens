#pragma once

#include "chatservice.h"
#include <QTimer>
#include <QWebSocket>

class Wasd : public ChatService
{
    Q_OBJECT
public:
    explicit Wasd(QSettings& settings, const QString& settingsGroupPath, QNetworkAccessManager& network, QObject *parent = nullptr);

    ConnectionStateType getConnectionStateType() const override;
    QString getStateDescription() const override;

protected:
    void reconnectImpl() override;

private slots:
    void onWebSocketReceived(const QString &rawData);

private:
    static QString extractChannelName(const QString& stream);

    QNetworkAccessManager& network;
    QWebSocket socket;
};
