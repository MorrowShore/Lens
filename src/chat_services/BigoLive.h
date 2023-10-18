#pragma once

#include "chatservice.h"
#include <QWebSocket>
#include <QTimer>
#include <QNetworkAccessManager>

class BigoLive : public ChatService
{
    Q_OBJECT
public:
    explicit BigoLive(ChatManager& manager, QSettings& settings, const QString& settingsGroupPathParent, QNetworkAccessManager& network, cweqt::Manager& web, QObject *parent = nullptr);

    ConnectionState getConnectionState() const override;
    QString getMainError() const override;

protected:
    void reconnectImpl() override;

private slots:
    void onWebSocketReceived(const QString& raw);

private:
    QNetworkAccessManager& network;
    QWebSocket socket;
};
