#pragma once

#include "chatservice.h"
#include <QWebSocket>
#include <QTimer>

class DLive : public ChatService
{
    Q_OBJECT
public:
    explicit DLive(QSettings& settings, const QString& settingsGroupPathParent, QNetworkAccessManager& network, cweqt::Manager& web, QObject *parent = nullptr);

    ConnectionStateType getConnectionStateType() const override;
    QString getStateDescription() const override;

protected:
    void reconnectImpl() override;

private slots:
    void send(const QString& type, const QJsonObject& payload = QJsonObject(), const int64_t id = -1);

    void onWebSocketReceived(const QString& text);

    void requestLivestreamPage(const QString& displayName);

private:
    struct UserInfo
    {
        QString userName;
        QString displayName;
        QString avatar;
    };

    struct Info
    {
        UserInfo owner;
        QString permalink;
    };

    static QString extractChannelName(const QString& stream);

    QNetworkAccessManager& network;
    QWebSocket socket;

    Info info;

    QHash<QString, UserInfo> users; // key - username
};
