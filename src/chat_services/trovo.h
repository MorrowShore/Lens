#pragma once

#include "chatservice.h"
#include <QWebSocket>
#include <QTimer>

class Trovo : public ChatService
{
    Q_OBJECT
public:
    explicit Trovo(ChatManager& manager, QSettings& settings, const QString& settingsGroupPathParent, QNetworkAccessManager& network, cweqt::Manager& web, QObject *parent = nullptr);
    
    ConnectionState getConnectionState() const override;
    QString getMainError() const override;

    QUrl requesGetAOuthTokenUrl() const;

protected:
    void resetImpl() override;
    void connectImpl() override;

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

    QNetworkAccessManager& network;

    QWebSocket socket;

    QString oauthToken;
    QString channelId;

    QTimer timerPing;
    QTimer timerUpdateChannelInfo;

    inline static const QString SmilesValidSymbols = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_0123456789!.";
    QHash<QString, QUrl> smiles;
};

