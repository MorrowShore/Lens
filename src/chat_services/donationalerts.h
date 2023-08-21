#pragma once

#include "chatservice.h"
#include "oauth2.h"
#include <QWebSocket>
#include <QJsonArray>
#include <QTimer>

class DonationAlerts : public ChatService
{
    Q_OBJECT
public:
    explicit DonationAlerts(QSettings& settings, const QString& settingsGroupPathParent, QNetworkAccessManager& network, cweqt::Manager& web, QObject *parent = nullptr);

    ConnectionStateType getConnectionStateType() const override;
    QString getStateDescription() const override;
    TcpReply processTcpRequest(const TcpRequest &request) override;

signals:

private slots:
    void updateUI();
    void requestDonations();
    void requestUser();
    void requestSubscribeCentrifuge(const QString& clientId, const QString& userId);
    void onReceiveWebSocket(const QString& rawData);

protected:
    void reconnectImpl() override;

private:
    struct PrivateChannelInfo
    {
        QString channel;
        QString token;
    };

    struct Info {
        int64_t lastMessageId = 0;
        QString socketConnectionToken;
        QString userId;
        QString userName;
    };

    void send(const QJsonObject& params, const int method = -1);
    void sendConnectToChannels(const QList<PrivateChannelInfo>& channels);
    void sendPing();

    void parseEvent(const QJsonObject& data);

    QNetworkAccessManager& network;
    OAuth2 auth;
    QWebSocket socket;

    QTimer timerReconnect;
    QTimer pingTimer;
    QTimer checkPingTimer;

    Info info;
    
    std::shared_ptr<UIBridgeElement> authStateInfo;
    std::shared_ptr<UIBridgeElement> loginButton;
    std::shared_ptr<UIBridgeElement> donateMainPageButton;
    std::shared_ptr<UIBridgeElement> donateAlternativePageButton;
};
