#pragma once

#include "../chatservice.h"
#include <QNetworkAccessManager>
#include <QTimer>
#include <memory>

class YouTube : public ChatService
{
    Q_OBJECT
public:
    explicit YouTube(ChatManager& manager, QSettings& settings, const QString& settingsGroupPathParent, QNetworkAccessManager& network, cweqt::Manager& web, QObject *parent = nullptr);

    ConnectionState getConnectionState() const override;
    QString getMainError() const override;

protected:
    void reconnectImpl() override;

private slots:
    void requestChatPage();
    void onReplyChatPage();

    void requestChatByContinuation();

    void requestStreamPage();
    void onReplyStreamPage();

private:
    struct Info
    {
        int badChatPageReplies = 0;
        int badLivePageReplies = 0;
        QString continuation;
    };

    void processBadChatReply();
    void processBadLivePageReply();

    QColor intToColor(quint64 rawColor) const;

    QNetworkAccessManager& network;

    QTimer timerRequestChat;
    QTimer timerRequestStreamPage;

    Info info;
};
