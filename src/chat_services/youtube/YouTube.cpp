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

static const int RequestChatInterval = 2000;
static const int RequestStreamInterval = 20000;

static const int MaxBadChatReplies = 10;
static const int MaxBadLivePageReplies = 3;

}

YouTube::YouTube(ChatManager& manager, QSettings& settings, const QString& settingsGroupPathParent, QNetworkAccessManager& network_, cweqt::Manager&, QObject *parent)
    : ChatService(manager, settings, settingsGroupPathParent, AxelChat::ServiceType::YouTube, false, parent)
    , network(network_)
{
    ui.findBySetting(stream)->setItemProperty("placeholderText", tr("Link or broadcast ID..."));

    QObject::connect(&timerRequestChat, &QTimer::timeout, this, &YouTube::requestChatPage);
    timerRequestChat.start(RequestChatInterval);

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

    requestChatPage();
    requestStreamPage();
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

void YouTube::requestChatPage()
{
    if (!isEnabled() || state.chatUrl.isEmpty())
    {
        return;
    }

    QNetworkRequest request(state.chatUrl);
    request.setHeader(QNetworkRequest::KnownHeaders::UserAgentHeader, QtAxelChatUtils::UserAgentNetworkHeaderName);
    request.setRawHeader("Accept-Language", YouTubeUtils::AcceptLanguageNetworkHeaderName);
    QObject::connect(network.get(request), &QNetworkReply::finished, this, &YouTube::onReplyChatPage);
}

void YouTube::onReplyChatPage()
{
    QNetworkReply *reply = dynamic_cast<QNetworkReply*>(sender());
    if (!reply)
    {
        qCritical() << "reply is null";
        return;
    }

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

    const int start = rawData.indexOf("\"actions\":[");
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
    if (!reply)
    {
        qCritical() << "!reply";
        return;
    }

    QObject::connect(reply, &QNetworkReply::finished, this, &YouTube::onReplyStreamPage);
}

void YouTube::onReplyStreamPage()
{
    QNetworkReply *reply = dynamic_cast<QNetworkReply*>(sender());
    if (!reply)
    {
        qCritical() << "!reply";
        return;
    }

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
