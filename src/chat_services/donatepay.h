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

protected:
    void reconnectImpl() override;

private:
    QNetworkAccessManager& network;
    QWebSocket socket;
};
