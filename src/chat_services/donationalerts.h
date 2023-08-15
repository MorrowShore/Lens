#pragma once

#include "chatservice.h"
#include "oauth2.h"
#include <QWebSocket>

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
    void onAuthStateChanged();
    void requestDonations();
    void requestUser();

protected:
    void reconnectImpl() override;

private:
    QNetworkAccessManager& network;
    OAuth2 auth;
    QWebSocket socket;

    std::shared_ptr<UIElementBridge> authStateInfo;
    std::shared_ptr<UIElementBridge> loginButton;
};
