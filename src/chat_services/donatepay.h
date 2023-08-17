#pragma once

#include "chatservice.h"
#include <QWebSocket>
#include <QJsonArray>
#include <QTimer>

class DonatePay : public ChatService
{
    Q_OBJECT
public:
    explicit DonatePay(QSettings& settings, const QString& settingsGroupPathParent, QNetworkAccessManager& network, cweqt::Manager& web, QObject *parent = nullptr);

    ConnectionStateType getConnectionStateType() const override;
    QString getStateDescription() const override;

signals:

private slots:
    void updateUI();
    void requestUser();
    void requestSocketToken();
    void requestSubscribeCentrifuge(const QString& clientId, const QString& userId);
    void onReceiveWebSocket(const QString& rawData);

protected:
    void reconnectImpl() override;
    void onUiElementChangedImpl(const std::shared_ptr<UIElementBridge> element) override;

private:
    struct Info
    {
        int64_t lastMessageId = 0;
        QString userId;
        QString userName;
        QString socketToken;
    };

    void send(const QJsonObject& params, const int method = -1);
    void sendConnectToChannels(const QString& socketToken, const QString& client);
    void sendPing();

    QNetworkAccessManager& network;
    QWebSocket socket;
    Setting<QString> apiKey;
    const QString domain;
    const QString donationPagePrefix;

    QTimer timerReconnect;
    QTimer pingTimer;
    QTimer checkPingTimer;

    std::shared_ptr<UIElementBridge> authStateInfo;
    std::shared_ptr<UIElementBridge> donatePageButton;

    Info info;
};
