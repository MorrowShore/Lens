#ifndef GOODGAME_H
#define GOODGAME_H

#include "chatservice.hpp"
#include "types.hpp"
#include <QObject>
#include <QWebSocket>
#include <QTimer>
#include <QJsonDocument>
#include <QSettings>

class GoodGame : public ChatService
{
    Q_OBJECT
public:
    struct Info {
        bool connected = false;
        double protocolVersion = 0;

        QString channelName;
        uint64_t channelId = 0;
    };

    explicit GoodGame(QSettings& settings, const QString& SettingsGroupPath, QNetworkAccessManager& network, QObject *parent = nullptr);
    ~GoodGame();

    ConnectionStateType getConnectionStateType() const override;
    QString getStateDescription() const override;
    int getViewersCount() const override;
    void reconnect() override;
    void setBroadcastLink(const QString &link) override;
    QString getBroadcastLink() const override;
    const Info& getInfo() const { return _info; }

signals:

private slots:
    void onWebSocketReceived(const QString& rawData);
    void sendToWebSocket(const QJsonDocument& data);
    void timeoutReconnect();

    void requestAuth();
    void requestGetChannelHistory();
    void requestChannelId();

private:

    QWebSocket _socket;

    QSettings& settings;
    const QString SettingsGroupPath;
    QNetworkAccessManager& network;

    Info _info;
    QString _lastConnectedChannelName;

    QTimer _timerReconnect;
};

#endif // GOODGAME_H
