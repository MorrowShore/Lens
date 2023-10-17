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
    void requestChat();

    void requestChatByChatPage();
    void requestChatByContinuation();

    void requestStreamPage();
    void onReplyStreamPage();

private:
    enum class ChatSource { ByChatPage, ByContinuation };

    struct Info
    {
        int badChatPageReplies = 0;
        int badLivePageReplies = 0;
        QString continuation;

        ChatSource chatSource = ChatSource::ByChatPage;
    };

    void setChatSource(const ChatSource source);
    void processBadChatReply();
    void processBadLivePageReply();

    QColor intToColor(quint64 rawColor) const;

    QNetworkAccessManager& network;

    QTimer timerRequestChat;
    QTimer timerRequestStreamPage;

    Info info;
};
