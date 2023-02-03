#pragma once

#include "chatservice.h"
#include <QTcpServer>
#include <QWebSocket>
#include <QTimer>

class Discord : public ChatService
{
    Q_OBJECT
public:
    explicit Discord(QSettings& settings, const QString& settingsGroupPath, QNetworkAccessManager& network, QObject *parent = nullptr);

    ConnectionStateType getConnectionStateType() const override;
    QString getStateDescription() const override;

protected:
    void reconnectImpl() override;

private slots:
    void onWebSocketReceived(const QString& rawData);
    void sendHeartbeat();

private:
    bool isAuthorized() const;
    void updateAuthState();
    void processDisconnected();

    void send(const QJsonObject& data);
    void parseHello(const QJsonObject& data);

    struct Info
    {
        int heartbeatInterval = 30000;
        QJsonValue  lastSequenceNumber;
    };

    QSettings& settings;
    QNetworkAccessManager& network;

    QTcpServer authServer;

    Info info;

    std::shared_ptr<UIElementBridge> authStateInfo;
    std::shared_ptr<UIElementBridge> loginButton;

    Setting<QString> oauthToken;
    Setting<QString> channel;

    QWebSocket socket;

    QTimer timerReconnect;
    QTimer heartbeatTimer;
    QTimer heartbeatAcknowledgementTimer;
};
