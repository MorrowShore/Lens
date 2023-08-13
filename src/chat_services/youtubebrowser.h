#pragma once

#include "chatservice.h"
#include <QNetworkAccessManager>
#include <QTimer>
#include <memory>

class YouTubeBrowser : public ChatService
{
    Q_OBJECT
public:
    explicit YouTubeBrowser(QSettings& settings, const QString& settingsGroupPathParent, QNetworkAccessManager& network, cweqt::Manager& web, QObject *parent = nullptr);

    ConnectionStateType getConnectionStateType() const override;
    QString getStateDescription() const override;

protected:
    void reconnectImpl() override;

public slots:
    void openWindow();

private:
    cweqt::Manager& web;
    std::shared_ptr<cweqt::Browser> browser;

    QTimer timerReconnect;
    QTimer timerCheckConnection;
};
