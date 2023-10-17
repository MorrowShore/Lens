#include "YouTube.h"
#include "utils/QtAxelChatUtils.h"
#include "utils/QtStringUtils.h"
#include "youtubeutils.h"
#include "models/messagesmodel.h"
#include "models/author.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrlQuery>
#include <QFile>
#include <QDir>
#include <QNetworkRequest>
#include <QNetworkReply>

namespace
{

static const int RequestChatByChatPageInterval = 2000;
static const int RequestChatByContinuationInterval = 1000;
static const int RequestStreamInterval = 20000;

static const int MaxBadChatReplies = 10;
static const int MaxBadLivePageReplies = 3;

}

YouTube::YouTube(ChatManager& manager, QSettings& settings, const QString& settingsGroupPathParent, QNetworkAccessManager& network_, cweqt::Manager&, QObject *parent)
    : ChatService(manager, settings, settingsGroupPathParent, AxelChat::ServiceType::YouTube, false, parent)
    , network(network_)
{
    ui.findBySetting(stream)->setItemProperty("placeholderText", tr("Link or broadcast ID..."));

    QObject::connect(&timerRequestChat, &QTimer::timeout, this, &YouTube::requestChat);

    QObject::connect(&timerRequestStreamPage,&QTimer::timeout, this, &YouTube::requestStreamPage);
    timerRequestStreamPage.start(RequestStreamInterval);
}

void YouTube::reconnectImpl()
{
    info = Info();

    state.streamId = YouTubeUtils::extractBroadcastId(stream.get().trimmed());

    if (!state.streamId.isEmpty())
    {
        state.chatUrl = QUrl(QString("https://www.youtube.com/live_chat?v=%1").arg(state.streamId));

        state.streamUrl = QUrl(QString("https://www.youtube.com/watch?v=%1").arg(state.streamId));

        state.controlPanelUrl = QUrl(QString("https://studio.youtube.com/video/%1/livestreaming").arg(state.streamId));
    }

    if (!isEnabled())
    {
        return;
    }

    setChatSource(ChatSource::ByChatPage);

    requestChat();
    requestStreamPage();
}

void YouTube::requestChat()
{
    if (!isEnabled())
    {
        return;
    }

    switch(info.chatSource)
    {
    case ChatSource::ByChatPage:
        //qDebug() << "request by chat page";
        requestChatByChatPage();
        break;

    case ChatSource::ByContinuation:
        //qDebug() << "request by chat continuation";
        requestChatByContinuation();
        break;
    }
}

ChatService::ConnectionState YouTube::getConnectionState() const
{
    if (isConnected())
    {
        return ChatService::ConnectionState::Connected;
    }
    else if (isEnabled() && !state.streamId.isEmpty())
    {
        return ChatService::ConnectionState::Connecting;
    }

    return ChatService::ConnectionState::NotConnected;
}

QString YouTube::getMainError() const
{
    if (stream.get().isEmpty())
    {
        return tr("Broadcast not specified");
    }

    if (state.streamId.isEmpty())
    {
        return tr("The broadcast is not correct");
    }

    return tr("Not connected");
}

void YouTube::requestChatByChatPage()
{
    if (!isEnabled() || state.chatUrl.isEmpty())
    {
        return;
    }

    QNetworkRequest request(state.chatUrl);
    request.setHeader(QNetworkRequest::KnownHeaders::UserAgentHeader, QtAxelChatUtils::UserAgentNetworkHeaderName);
    request.setRawHeader("Accept-Language", YouTubeUtils::AcceptLanguageNetworkHeaderName);
    QNetworkReply* reply = network.get(request);
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]()
    {
        const QByteArray rawData = reply->readAll();
        reply->deleteLater();

        if (rawData.isEmpty())
        {
            processBadChatReply();
            qCritical() << "raw data is empty";
            return;
        }

        //AxelChat::saveDebugDataToFile(FolderLogs, "raw_last_youtube.html", rawData);

        const QString startData = QString::fromUtf8(rawData.left(100));

        if (startData.contains("<title>Oops</title>", Qt::CaseSensitivity::CaseInsensitive))
        {
            //ToDo: show message "bad url"
            processBadChatReply();
            return;
        }

        int startFindPos = 0;

        {
            static const QString Prefix = "\"continuation\":\"";
            if (const int start = rawData.indexOf(Prefix.toLatin1()) + Prefix.length(); start != -1)
            {
                const int end = rawData.indexOf('"', start);

                info.continuation = rawData.mid(start, end - start);

                startFindPos = end;

                if (info.continuation.isEmpty())
                {
                    qWarning() << "continuation is empty";
                }
                else
                {
                    setChatSource(ChatSource::ByContinuation);
                }
            }
            else
            {
                qWarning() << "continuation not found";
            }
        }

        const int start = rawData.indexOf("\"actions\":[", startFindPos);
        if (start == -1)
        {
            qCritical() << "not found actions";
            QtStringUtils::saveDebugDataToFile(YouTubeUtils::FolderLogs, "not_found_actions_from_html_youtube.html", rawData);
            processBadChatReply();
            return;
        }

        QByteArray data = rawData.mid(start + 10);
        const int pos = data.lastIndexOf(",\"actionPanel\"");
        if (pos != -1)
        {
            data = data.remove(pos, data.length());
        }

        //AxelChat::saveDebugDataToFile(FolderLogs, "last_youtube.json", data);

        const QJsonDocument jsonDocument = QJsonDocument::fromJson(data);
        if (jsonDocument.isArray())
        {
            if (!isConnected() && !state.streamId.isEmpty() && isEnabled())
            {
                setConnected(true);
            }

            const QJsonArray actionsArray = jsonDocument.array();
            //qDebug() << "array size = " << actionsArray.size();

            QList<std::shared_ptr<Message>> messages;
            QList<std::shared_ptr<Author>> authors;

            YouTubeUtils::parseActionsArray(actionsArray, data, messages, authors);

            if (!messages.isEmpty())
            {
                emit readyRead(messages, authors);
                emit stateChanged();
            }

            info.badChatPageReplies = 0;
        }
        else
        {
            YouTubeUtils::printData(Q_FUNC_INFO + QString(": document is not array"), data);

            QtStringUtils::saveDebugDataToFile(YouTubeUtils::FolderLogs, "failed_to_parse_from_html_youtube.html", rawData);
            QtStringUtils::saveDebugDataToFile(YouTubeUtils::FolderLogs, "failed_to_parse_from_html_youtube.json", data);
            processBadChatReply();
        }
    });
}

void YouTube::requestChatByContinuation()
{
    if (!isEnabled() || state.chatUrl.isEmpty())
    {
        return;
    }

    if (info.continuation.isEmpty())
    {
        qCritical() << "continuation is empty";
        return;
    }

    const QJsonDocument doc(QJsonObject(
        {
            { "context", QJsonObject({
                    { "client", QJsonObject({
                        { "clientName", "WEB" },
                        { "clientVersion", "2.20231016.01.00" },
                    })}
                })},
            { "continuation", info.continuation },
        }));

    QNetworkRequest request(QUrl("https://www.youtube.com/youtubei/v1/live_chat/get_live_chat"));
    request.setHeader(QNetworkRequest::KnownHeaders::UserAgentHeader, QtAxelChatUtils::UserAgentNetworkHeaderName);
    request.setRawHeader("Accept-Language", YouTubeUtils::AcceptLanguageNetworkHeaderName);
    request.setRawHeader("Content-Type", "application/json");
    QNetworkReply* reply = network.post(request, doc.toJson());
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]()
    {
        const QByteArray data = reply->readAll();

        const QJsonObject root = QJsonDocument::fromJson(data).object();

        const QJsonObject liveChatContinuation = root
            .value("continuationContents").toObject()
            .value("liveChatContinuation").toObject();

        {
            if (const QJsonValue actions = liveChatContinuation.value("actions"); actions.isArray())
            {
                const QJsonArray actionsArray = actions.toArray();

                QList<std::shared_ptr<Message>> messages;
                QList<std::shared_ptr<Author>> authors;

                YouTubeUtils::parseActionsArray(actionsArray, data, messages, authors);

                if (!messages.isEmpty())
                {
                    emit readyRead(messages, authors);
                    emit stateChanged();
                }

                info.badChatPageReplies = 0;
            }
            else
            {
                processBadChatReply();
            }
        }

        {
            const QJsonArray continuations = liveChatContinuation
                .value("continuations").toArray();
            if (continuations.isEmpty())
            {
                qWarning() << "continuations not found";

                info.continuation = QString();

                setChatSource(ChatSource::ByChatPage);
                requestChat();
                return;
            }

            info.continuation = continuations.at(0).toObject()
                                    .value("invalidationContinuationData").toObject()
                                    .value("continuation").toString();

            if (info.continuation.isEmpty())
            {
                qWarning() << "continuation is empty";

                setChatSource(ChatSource::ByChatPage);
                requestChat();
                return;
            }
        }
    });
}

void YouTube::requestStreamPage()
{
    if (!isEnabled() || state.streamUrl.isEmpty())
    {
        return;
    }

    QNetworkRequest request(state.streamUrl);
    request.setHeader(QNetworkRequest::KnownHeaders::UserAgentHeader, QtAxelChatUtils::UserAgentNetworkHeaderName);
    request.setRawHeader("Accept-Language", YouTubeUtils::AcceptLanguageNetworkHeaderName);

    QNetworkReply* reply = network.get(request);
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]()
    {
        const QByteArray rawData = reply->readAll();
        reply->deleteLater();

        if (rawData.isEmpty())
        {
            qCritical() << "raw data is empty";
            processBadLivePageReply();
            return;
        }

        const int viewers = YouTubeUtils::parseViews(rawData);
        setViewers(viewers);

        if (viewers != -1)
        {
            info.badLivePageReplies = 0;
        }
        else
        {
            processBadLivePageReply();
        }
    });
}

void YouTube::setChatSource(const ChatSource source)
{
    switch (source) {
    case ChatSource::ByChatPage:
        info.continuation = QString();
        timerRequestChat.stop();
        timerRequestChat.start(RequestChatByChatPageInterval);
        break;

    case ChatSource::ByContinuation:
        timerRequestChat.stop();
        timerRequestChat.start(RequestChatByContinuationInterval);
        break;
    }

    info.chatSource = source;

    //qDebug() << "setted chat source =" << (int)source;
}

void YouTube::processBadChatReply()
{
    info.badChatPageReplies++;

    if (info.badChatPageReplies >= MaxBadChatReplies)
    {
        if (isConnected() && !state.streamId.isEmpty())
        {
            qWarning() << "too many bad chat replies! Disonnecting...";
            setConnected(false);
        }
    }
}

void YouTube::processBadLivePageReply()
{
    info.badLivePageReplies++;

    if (info.badLivePageReplies >= MaxBadLivePageReplies)
    {
        qWarning() << "too many bad live page replies!";
        setViewers(-1);
    }
}

QColor YouTube::intToColor(quint64 rawColor) const
{
    qDebug() << "raw color" << rawColor;
    return QColor::fromRgba64(QRgba64::fromRgba64(rawColor));
}
