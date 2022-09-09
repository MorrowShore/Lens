#ifndef GOODGAME_H
#define GOODGAME_H

#include "chatservice.h"
#include "utils.h"
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

    ConnectionStateType getConnectionStateType() const override;
    QString getStateDescription() const override;
    void reconnect() override;

signals:

protected:
    void onParameterChanged(Parameter &parameter) override;

private slots:
    void onWebSocketReceived(const QString& rawData);
    void sendToWebSocket(const QJsonDocument& data);

    void requestAuth();
    void requestChannelHistory();
    void requestChannelStatus();
    void requestUserPage(const QString& authorName, const QString& authorId);

private:
    static QString getStreamId(const QString& stream);

    QWebSocket _socket;

    QSettings& settings;
    QNetworkAccessManager& network;

    int64_t channelId = -1;
    QString _lastConnectedChannelName;

    QTimer timerUpdateMessages;
    QTimer timerUpdateChannelStatus;

    QSet<QString> requestedInfoUsers;
};

#endif // GOODGAME_H
