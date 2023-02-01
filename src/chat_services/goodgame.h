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
    explicit GoodGame(QSettings& settings, const QString& settingsGroupPath, QNetworkAccessManager& network, QObject *parent = nullptr);

    ConnectionStateType getConnectionStateType() const override;
    QString getStateDescription() const override;

signals:

protected:
    void reconnectImpl() override;

private slots:
    void onWebSocketReceived(const QString& rawData);
    void sendToWebSocket(const QJsonDocument& data);

    void requestAuth();
    void requestChannelHistory();
    void requestChannelStatus();
    void requestUserPage(const QString& authorName, const QString& authorId);
    void requestSmiles();

private:
    static QString getStreamId(const QString& stream);

    QWebSocket socket;

    QSettings& settings;
    QNetworkAccessManager& network;

    int64_t channelId = -1;
    QString lastConnectedChannelName;

    QTimer timerReconnect;
    QTimer timerUpdateMessages;
    QTimer timerUpdateChannelStatus;

    QSet<QString> requestedInfoUsers;

    inline static const QString SmilesValidSymbols = "abcdefghijklmnopqrstuvwxyz_0123456789";
    QHash<QString, QUrl> smiles;
};

#endif // GOODGAME_H
