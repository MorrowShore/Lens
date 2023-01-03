#include "vkplaylive.h"
#include "models/message.h"
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

namespace
{

static const int RequestChatInterval = 2000;

}

VkPlayLive::VkPlayLive(QSettings& settings_, const QString& settingsGroupPath, QNetworkAccessManager& network_, QObject *parent)
    : ChatService(settings_, settingsGroupPath, AxelChat::ServiceType::VkPlayLive, parent)
    , settings(settings_)
    , network(network_)
{
    getParameter(stream)->setPlaceholder(tr("Link or channel name..."));

    QObject::connect(&timerRequestChat, &QTimer::timeout, this, &VkPlayLive::onTimeoutRequestChat);
    timerRequestChat.start(RequestChatInterval);

    reconnect();
}

ChatService::ConnectionStateType VkPlayLive::getConnectionStateType() const
{
    if (state.connected)
    {
        return ChatService::ConnectionStateType::Connected;
    }
    else if (!state.streamId.isEmpty())
    {
        return ChatService::ConnectionStateType::Connecting;
    }

    return ChatService::ConnectionStateType::NotConnected;
}

QString VkPlayLive::getStateDescription() const
{
    switch (getConnectionStateType())
    {
    case ConnectionStateType::NotConnected:
        if (stream.get().isEmpty())
        {
            return tr("Stream not specified");
        }

        if (state.streamId.isEmpty())
        {
            return tr("The stream is not correct");
        }

        return tr("Not connected");

    case ConnectionStateType::Connecting:
        return tr("Connecting...");

    case ConnectionStateType::Connected:
        return tr("Successfully connected!");

    }

    return "<unknown_state>";
}

void VkPlayLive::reconnect()
{
    const bool preConnected = state.connected;
    const QString preStreamId = state.streamId;

    state = State();

    state.connected = false;

    state.streamId = extractChannelName(stream.get().trimmed());

    if (!state.streamId.isEmpty())
    {
        state.chatUrl = QUrl(QString("https://vkplay.live/%1/only-chat").arg(state.streamId));

        state.streamUrl = QUrl(QString("https://vkplay.live/%1").arg(state.streamId));

        //state.controlPanelUrl = QUrl(QString("https://studio.youtube.com/video/%1/livestreaming").arg(state.streamId));
    }

    if (preConnected && !preStreamId.isEmpty())
    {
        emit connectedChanged(false, preStreamId);
    }

    emit stateChanged();

    onTimeoutRequestChat();
}

void VkPlayLive::onTimeoutRequestChat()
{
    if (state.streamId.isEmpty())
    {
        return;
    }

    QNetworkRequest request("https://api.vkplay.live/v1/blog/" + state.streamId + "/public_video_stream/chat?limit=20");
    request.setHeader(QNetworkRequest::KnownHeaders::UserAgentHeader, AxelChat::UserAgentNetworkHeaderName);
    QNetworkReply* reply = network.get(request);
    if (!reply)
    {
        qDebug() << Q_FUNC_INFO << ": !reply";
        return;
    }

    QObject::connect(reply, &QNetworkReply::finished, this, &VkPlayLive::onReplyChat);
}

void VkPlayLive::onReplyChat()
{
    QNetworkReply *reply = dynamic_cast<QNetworkReply*>(sender());
    if (!reply)
    {
        qDebug() << "!reply";
        return;
    }

    const QByteArray replyData = reply->readAll();
    qDebug(replyData);

    QList<Message> messages;
    QList<Author> authors;

    const QJsonObject root = QJsonDocument::fromJson(replyData).object();
    reply->deleteLater();

    const QJsonArray rawMessages = root.value("data").toArray();
    for (const QJsonValue& value : qAsConst(rawMessages))
    {
        if (!state.connected && !state.streamId.isEmpty())
        {
            state.connected = true;

            emit connectedChanged(true, state.streamId);
            emit stateChanged();
        }

        const QJsonObject rawMessage = value.toObject();
        const QJsonObject rawAuthor = rawMessage.value("author").toObject();

        const QString authorName = rawAuthor.value("displayName").toString();
        const QString authorAvatarUrl = rawAuthor.value("avatarUrl").toString();
        const QString authorId = rawAuthor.value("id").toString();

        QDateTime publishedAt;

        bool ok = false;
        const int64_t rawSendTime = rawMessage.value("createdAt").toVariant().toLongLong(&ok);
        if (ok)
        {
            publishedAt = QDateTime::fromSecsSinceEpoch(rawSendTime);
        }
        else
        {
            publishedAt = QDateTime::currentDateTime();
        }

        const QString messageId = QString("%1").arg(rawMessage.value("id").toVariant().toLongLong());

        QList<Message::Content*> contents;

        const QJsonArray messageData = rawMessage.value("data").toArray();
        for (const QJsonValue& value : qAsConst(messageData))
        {
            const QJsonObject data = value.toObject();
            const QString type = data.value("type").toString();

            if (type == "text")
            {
                const QString rawContent = data.value("content").toString();
                if (rawContent.isEmpty())
                {
                    continue;
                }

                const QJsonArray rawContents = QJsonDocument::fromJson(rawContent.toUtf8()).array();
                if (rawContents.isEmpty())
                {
                    continue;
                }

                const QString text = rawContents.at(0).toString();

                contents.append(new Message::Text(text));
            }
            else
            {
                //TODO
            }
        }

        Author author(getServiceType(),
                      authorName,
                      getServiceTypeId(serviceType) + QString("/%1").arg(authorId),
                      authorAvatarUrl,
                      QUrl("https://vkplay.live/" + authorName));

        Message message(contents, author, publishedAt, QDateTime::currentDateTime(), getServiceTypeId(serviceType) + QString("/%1").arg(messageId));

        messages.append(message);
        authors.append(author);
    }

    if (!messages.isEmpty())
    {
        emit readyRead(messages, authors);
    }
}

QString VkPlayLive::extractChannelName(const QString &stream)
{
    return stream;
}
