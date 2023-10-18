#pragma once

#include "chatservice.h"
#include <QNetworkAccessManager>
#include <QTimer>

class Rutube : public ChatService
{
    Q_OBJECT
public:
    explicit Rutube(ChatManager& manager, QSettings& settings, const QString& settingsGroupPathParent, QNetworkAccessManager& network, cweqt::Manager& web, QObject *parent = nullptr);

    ConnectionState getConnectionState() const override;
    QString getMainError() const override;

protected:
    void reconnectImpl() override;

private slots:
    void requestChat();
    void requestViewers();
    void processBadChatReply();

private:
    struct Info
    {
        int badChatPageReplies = 0;
    };

    QString extractBroadcastId(const QString& raw);
    QPair<std::shared_ptr<Message>, std::shared_ptr<Author>> parseResult(const QJsonObject& result);

    QNetworkAccessManager &network;

    Info info;

    QTimer timerRequestChat;
    QTimer timerRequestViewers;
};
