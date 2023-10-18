#include "Rutube.h"
#include "utils/QtStringUtils.h"
#include "models/message.h"
#include "models/author.h"
#include <QNetworkReply>
#include <QNetworkRequest>

namespace
{

static const int RequestChatInterval = 2000;
static const int RequestViewersInterval = 10000;
static const int MaxBadChatReplies = 3;
static const QString VerifiedBadge = "qrc:/resources/images/rutube-verified.png";

}

Rutube::Rutube(ChatManager &manager, QSettings &settings, const QString &settingsGroupPathParent, QNetworkAccessManager &network_, cweqt::Manager &, QObject *parent)
    : ChatService(manager, settings, settingsGroupPathParent, AxelChat::ServiceType::Rutube, false, parent)
    , network(network_)
{
    ui.findBySetting(stream)->setItemProperty("placeholderText", tr("Broadcast link..."));

    QObject::connect(&timerRequestChat, &QTimer::timeout, this, &Rutube::requestChat);
    timerRequestChat.start(RequestChatInterval);

    QObject::connect(&timerRequestViewers, &QTimer::timeout, this, &Rutube::requestViewers);
    timerRequestViewers.start(RequestViewersInterval);
}

ChatService::ConnectionState Rutube::getConnectionState() const
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

QString Rutube::getMainError() const
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

void Rutube::reconnectImpl()
{
    info = Info();

    state.streamId = extractBroadcastId(stream.get().trimmed());

    if (!state.streamId.isEmpty())
    {
        state.streamUrl = QUrl(QString("https://rutube.ru/video/%1/").arg(state.streamId));

        state.controlPanelUrl = QUrl(QString("https://rutube.ru/livestreaming/%1/").arg(state.streamId));
    }

    if (!isEnabled())
    {
        return;
    }

    requestChat();
    requestViewers();
}

void Rutube::requestChat()
{
    if (!isEnabled() || state.streamId.isEmpty())
    {
        return;
    }

    QNetworkRequest request(QString("https://rutube.ru/api/chat/%1?direction=past&format=json&limit=10").arg(state.streamId));
    QNetworkReply* reply = network.get(request);

    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]()
    {
        const QByteArray rawData = reply->readAll();
        reply->deleteLater();

        const QJsonObject root = QJsonDocument::fromJson(rawData).object();
        const QJsonValue resultValue = root.value("results");
        if (!resultValue.isArray())
        {
            processBadChatReply();
            qWarning() << "no results, root =" << root;
            return;
        }

        info.badChatPageReplies = 0;

        if (!isConnected() && !state.streamId.isEmpty() && isEnabled())
        {
            setConnected(true);
            requestViewers();
        }

        QList<std::shared_ptr<Message>> messages;
        QList<std::shared_ptr<Author>> authors;

        const QJsonArray results = resultValue.toArray();
        for (const QJsonValue& v : results)
        {
            const auto pair = parseResult(v.toObject());

            if (pair.first && pair.second)
            {
                messages.append(pair.first);
                authors.append(pair.second);
            }
        }

        if (!messages.isEmpty() && !authors.isEmpty())
        {
            std::reverse(messages.begin(), messages.end());
            std::reverse(authors.begin(), authors.end());

            emit readyRead(messages, authors);
        }
    });
}

void Rutube::requestViewers()
{
    if (!isEnabled() || state.streamId.isEmpty() || !isConnected())
    {
        return;
    }

    QNetworkRequest request(QString("https://goya.rutube.ru/video/%1/?online=1").arg(state.streamId));
    QNetworkReply* reply = network.get(request);

    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]()
    {
        const QByteArray rawData = reply->readAll();
        reply->deleteLater();

        const QJsonObject root = QJsonDocument::fromJson(rawData).object();

        setViewers(root.value("views_online").toInt(-1));
    });
}

void Rutube::processBadChatReply()
{
    if (!isEnabled() || state.streamId.isEmpty())
    {
        return;
    }

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

QString Rutube::extractBroadcastId(const QString &raw)
{
    const QString simpleUrl = QtStringUtils::simplifyUrl(raw);

    {
        static const QRegExp rx("^(studio\\.)?rutube.ru/.+/([^/]*)", Qt::CaseInsensitive);
        if (rx.indexIn(simpleUrl) != -1)
        {
            if (const QString result = rx.cap(2); !result.isEmpty())
            {
                return result;
            }
        }
    }

    {
        static const QRegExp rx("^[a-fA-F0-9]+$", Qt::CaseInsensitive);
        if (rx.indexIn(simpleUrl) != -1)
        {
            if (const QString result = rx.cap(0); !result.isEmpty())
            {
                return result;
            }
        }
    }

    return QString();
}

QPair<std::shared_ptr<Message>, std::shared_ptr<Author>> Rutube::parseResult(const QJsonObject &result)
{
    const QString type = result.value("type").toString();
    if (type != "message")
    {
        qWarning() << "unknown type" << type << ", result =" << result;
    }

    const QJsonObject payload = result.value("payload").toObject();

    const QJsonObject user = payload.value("user").toObject();
    const QString userRawId = QString("%1").arg(user.value("id").toVariant().toLongLong());
    const QString name = user.value("name").toString();
    const QString avatar = user.value("avatar_url").toString();
    const QString page = user.value("site_url").toString();
    const bool official = user.value("is_official").toBool();

    Author::Builder authorBuilder(getServiceType(), generateAuthorId(userRawId), name);

    authorBuilder
        .setAvatar(avatar)
        .setPage(page);

    if (official)
    {
        authorBuilder.addRightBadge(VerifiedBadge);
    }

    auto author = authorBuilder.build();

    const QString messageRawId = payload.value("id").toString();
    const QString text = payload.value("text").toString();
    const QDateTime publishedTime = QDateTime::fromSecsSinceEpoch(
        payload.value("created_ts_real").toVariant().toLongLong());

    auto message = Message::Builder(author, generateMessageId(messageRawId))
        .addText(text)
        .setPublishedTime(publishedTime)
        .build();

    return { message, author };
}
