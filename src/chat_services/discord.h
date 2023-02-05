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

protected:
    void onUiElementChangedImpl(const std::shared_ptr<UIElementBridge> element) override;
    void reconnectImpl() override;

private slots:
    void onWebSocketReceived(const QString& rawData);
    void sendHeartbeat();
    void sendIdentify();

private:
    void processDisconnected();
    void processConnected();

    void send(const int opCode, const QJsonValue& data);

    void parseHello(const QJsonObject& data);
    void parseDispatch(const QString& eventType, const QJsonObject& data);
    void parseMessageCreate(const QJsonObject& jsonMessage);

    struct Info
    {
        int heartbeatInterval = 30000;
        QJsonValue lastSequence;
    };

    QSettings& settings;
    QNetworkAccessManager& network;

    Info info;

    Setting<QString> botToken;

    QWebSocket socket;

    QTimer timerReconnect;
    QTimer heartbeatTimer;
    QTimer heartbeatAcknowledgementTimer;
};
