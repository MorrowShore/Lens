#pragma once

#include "chatservice.h"
#include <QWebSocket>

class DonationAlerts : public ChatService
{
    Q_OBJECT
public:
    explicit DonationAlerts(QSettings& settings, const QString& settingsGroupPathParent, QNetworkAccessManager& network, cweqt::Manager& web, QObject *parent = nullptr);

    ConnectionStateType getConnectionStateType() const override;
    QString getStateDescription() const override;

signals:

protected:
    void reconnectImpl() override;

private:
    QNetworkAccessManager& network;
    QWebSocket socket;
};
