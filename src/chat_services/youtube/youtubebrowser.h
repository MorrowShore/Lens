#pragma once

#include "../chatservice.h"
#include <QNetworkAccessManager>
#include <QTimer>
#include <memory>

class YouTubeBrowser : public ChatService
{
    Q_OBJECT
public:
    explicit YouTubeBrowser(ChatManager& manager, QSettings& settings, const QString& settingsGroupPathParent, QNetworkAccessManager& network, cweqt::Manager& web, QObject *parent = nullptr);
    
    ConnectionState getConnectionState() const override;
    QString getMainError() const override;

protected:
    void reconnectImpl() override;

public slots:
    void requestStreamPage();
    void openWindow();

private:
    QNetworkAccessManager& network;
    cweqt::Manager& web;
    std::shared_ptr<cweqt::Browser> browser;

    QTimer timerReconnect;
    QTimer timerCheckConnection;
    QTimer timerRequestStreamPage;
    
    std::shared_ptr<UIBridgeElement> openChatButton;
};
