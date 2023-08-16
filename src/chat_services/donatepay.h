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

protected:
    void reconnectImpl() override;
    void onUiElementChangedImpl(const std::shared_ptr<UIElementBridge> element) override;

private:
    struct Info
    {
        QString userId;
        QString userName;
    };

    QNetworkAccessManager& network;
    QWebSocket socket;
    Setting<QString> apiKey;
    const QString domain;
    const QString donationPagePrefix;

    std::shared_ptr<UIElementBridge> authStateInfo;
    std::shared_ptr<UIElementBridge> donatePageButton;

    Info info;
};
