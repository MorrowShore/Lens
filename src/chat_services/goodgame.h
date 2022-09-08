#ifndef GOODGAME_H
#define GOODGAME_H

#include "chatservice.hpp"
#include "utils.hpp"
#include <QObject>
#include <QWebSocket>
#include <QTimer>
#include <QJsonDocument>
#include <QSettings>

class GoodGame : public ChatService
{
    Q_OBJECT
public:
    explicit GoodGame(QSettings& settings, const QString& SettingsGroupPath, QNetworkAccessManager& network, QObject *parent = nullptr);
    ~GoodGame();

    ConnectionStateType getConnectionStateType() const override;
    QString getStateDescription() const override;
    void reconnect() override;

signals:

protected:
    void onParameterChanged(Parameter &parameter) override;

private slots:
    void onWebSocketReceived(const QString& rawData);
    void sendToWebSocket(const QJsonDocument& data);
    void timeoutReconnect();

    void requestAuth();
    void requestGetChannelHistory();
    void requestChannelId();

private:
    struct Info {
        bool connected = false;
        double protocolVersion = 0;

        QString channelName;
        uint64_t channelId = 0;
    };

    QWebSocket _socket;

    QSettings& settings;
    QNetworkAccessManager& network;

    Info _info;
    QString _lastConnectedChannelName;

    QTimer _timerReconnect;
};

#endif // GOODGAME_H
