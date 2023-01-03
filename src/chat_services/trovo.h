#pragma once

#include "chatservice.h"
#include <QWebSocket>
#include <QTimer>

class Trovo : public ChatService
{
    Q_OBJECT
public:
    explicit Trovo(QSettings& settings, const QString& settingsGroupPath, QNetworkAccessManager& network, QObject *parent = nullptr);

    ConnectionStateType getConnectionStateType() const override;
    QString getStateDescription() const override;
    void reconnect() override;

    QUrl requesGetAOuthTokenUrl() const;

private slots:
    void onWebSocketReceived(const QString& rawData);
    void sendToWebSocket(const QJsonDocument& data);
    void ping();

private:
    static QString getChannelName(const QString& stream);
    void requestChannelId();
    void requestChatToken();
    void requestChannelInfo();
    void requsetSmiles();

    QSettings& settings;
    QNetworkAccessManager& network;

    QWebSocket socket;

    QString oauthToken;
    QString channelId;

    QTimer timerPing;
    QTimer timerReconnect;
    QTimer timerUpdateChannelInfo;

    QString lastConnectedChannelName;

    inline static const QString SmilesValidSymbols = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_0123456789";
    QHash<QString, QUrl> smiles;
};

