#pragma once

#include "chatservice.h"
#include "utils.h"
#include <QObject>
#include <QWebSocket>
#include <QTimer>
#include <QJsonDocument>

class GoodGame : public ChatService
{
    Q_OBJECT
public:
    explicit GoodGame(ChatHandler& manager, QSettings& settings, const QString& settingsGroupPathParent, QNetworkAccessManager& network, cweqt::Manager& web, QObject *parent = nullptr);
    
    ConnectionState getConnectionState() const override;
    QString getMainError() const override;

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

    QNetworkAccessManager& network;

    int64_t channelId = -1;

    QTimer timerUpdateMessages;
    QTimer timerUpdateChannelStatus;

    QSet<QString> requestedInfoUsers;

    inline static const QString SmilesValidSymbols = "abcdefghijklmnopqrstuvwxyz_0123456789";
    QHash<QString, QUrl> smiles;
};
