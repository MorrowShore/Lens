#pragma once

#include "chatservice.h"

class NimoTV : public ChatService
{
public:
    explicit NimoTV(ChatManager& manager, QSettings& settings, const QString& settingsGroupPathParent, QNetworkAccessManager& network, cweqt::Manager& web, QObject *parent = nullptr);

    ConnectionState getConnectionState() const override;
    QString getMainError() const override;

protected:
    void reconnectImpl() override;
};
