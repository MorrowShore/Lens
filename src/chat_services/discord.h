#pragma once

#include "chatservice.h"
#include <QWebSocket>
#include <QTimer>

class Discord : public ChatService
{
    Q_OBJECT
public:
    explicit Discord(QSettings& settings, const QString& settingsGroupPath, QNetworkAccessManager& network, QObject *parent = nullptr);

    ConnectionStateType getConnectionStateType() const override;
    QString getStateDescription() const override;
    TcpReply processTcpRequest(const TcpRequest &request) override;

protected:
    void reconnectImpl() override;

private slots:
    void onWebSocketReceived(const QString& rawData);
    void sendHeartbeat();
    void sendIdentify();

private:
    bool isAuthorized() const;
    void updateAuthState();
    void processDisconnected();

    void send(const int opCode, const QJsonValue& data);

    void parseHello(const QJsonObject& data);
    void parseDispatch(const QString& eventType, const QJsonObject& data);

    struct Info
    {
        int heartbeatInterval = 30000;
        QJsonValue lastSequence;
    };

    QSettings& settings;
    QNetworkAccessManager& network;

    Info info;

    std::shared_ptr<UIElementBridge> authStateInfo;
    std::shared_ptr<UIElementBridge> loginButton;

    Setting<QString> oauthToken;

    QWebSocket socket;

    QTimer timerReconnect;
    QTimer heartbeatTimer;
    QTimer heartbeatAcknowledgementTimer;
};
