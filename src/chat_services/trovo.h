#pragma once

#include "chatservice.h"
#include "models/message.h"
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

    void parseContentAsText(const QJsonValue& jsonContent, Message::Builder& builder, const QJsonObject& contentData, const bool italic, const bool bold, const bool toUpperFirstChar) const;
    void parseTodo19(const QJsonValue& jsonContent, Message::Builder& builder) const;
    void parseSpell(const QJsonValue& jsonContent, Message::Builder& builder) const;

    static void replaceWithData(QString& text, const QJsonObject& contentData);

    static bool isEmote(const QString& chunk, const QString* prevChunk);

    QNetworkAccessManager& network;

    QWebSocket socket;

    Setting<bool> showWelcome;
    Setting<bool> showUnfollow;
    Setting<bool> showFollow;
    Setting<bool> showSubscription;

    QString oauthToken;
    QString channelId;

    QTimer timerPing;
    QTimer timerUpdateChannelInfo;

    QHash<QString, QUrl> smiles;
};

