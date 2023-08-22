#pragma once

#include "chatservice.h"
#include <QJsonObject>
#include <QTimer>

class Odysee : public ChatService
{
    Q_OBJECT
public:
    explicit Odysee(QSettings& settings, const QString& settingsGroupPathParent, QNetworkAccessManager& network, cweqt::Manager& web, QObject *parent = nullptr);

    ConnectionStateType getConnectionStateType() const override;
    QString getStateDescription() const override;

protected:
    void reconnectImpl() override;

private:
    struct Info
    {
        QString claimId;
    };

    void requestClaimId();
    void requestLive();

    QNetworkAccessManager& network;

    Info info;
};
