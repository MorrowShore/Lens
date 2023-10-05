#include "odysee.h"
#include "models/message.h"
#include "models/author.h"
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

namespace
{

static const int ReconncectPeriod = 5000;
static const int PingSendTimeout = 10 * 1000;
static const int CheckPingSendTimeout = PingSendTimeout * 1.5;

static const QUrl DefaultAvatar = QUrl("https://thumbnails.odycdn.com/optimize/s:160:160/quality:85/plain/https://spee.ch/spaceman-png:2.png");
static const QColor BackgroundDonationColor = QColor(255, 209, 147);
static const QColor CreatorNicknameBackgroundColor = QColor(166, 10, 67);

static bool checkReply(QNetworkReply *reply, const char *tag, QByteArray& resultData)
{
    resultData.clear();

    if (!reply)
    {
        qWarning() << tag << ": !reply";
        return false;
    }

    resultData = reply->readAll();
    reply->deleteLater();
    if (resultData.isEmpty())
    {
        qWarning() << tag << ": data is empty";
        return false;
    }

    return true;
}

}

Odysee::Odysee(QSettings &settings, const QString &settingsGroupPathParent, QNetworkAccessManager &network_, cweqt::Manager&, QObject *parent)
    : ChatService(settings, settingsGroupPathParent, AxelChat::ServiceType::Odysee, false, parent)
    , network(network_)
{
    ui.findBySetting(stream)->setItemProperty("name", tr("Stream"));
    ui.findBySetting(stream)->setItemProperty("placeholderText", tr("Stream link..."));

    QObject::connect(&socket, &QWebSocket::stateChanged, this, [](QAbstractSocket::SocketState state){
        Q_UNUSED(state)
        //qDebug() << "webSocket state changed:" << state;
    });

    QObject::connect(&socket, &QWebSocket::textMessageReceived, this, &Odysee::onReceiveWebSocket);

    QObject::connect(&socket, &QWebSocket::connected, this, [this]()
    {
        //qDebug() << "webSocket connected";

        if (!state.connected)
        {
            state.connected = true;
            emit stateChanged();

            sendPing();
        }

        checkPingTimer.setInterval(CheckPingSendTimeout);
        checkPingTimer.start();
    });

    QObject::connect(&socket, &QWebSocket::disconnected, this, [this]()
    {
        //qDebug() << "webSocket disconnected";

        if (state.connected)
        {
            state.connected = false;
            emit stateChanged();
        }
    });

    QObject::connect(&socket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error), this, [this](QAbstractSocket::SocketError error_){
        qDebug() << "webSocket error:" << error_ << ":" << socket.errorString();
    });

    QObject::connect(&timerReconnect, &QTimer::timeout, this, [this]()
    {
        if (!enabled.get())
        {
            return;
        }

        if (!state.connected)
        {
            reconnect();
        }
    });
    timerReconnect.start(ReconncectPeriod);

    reconnect();
}

ChatService::ConnectionState Odysee::getConnectionState() const
{
    if (state.connected)
    {
        return ChatService::ConnectionState::Connected;
    }
    else if (enabled.get() && !state.streamId.isEmpty())
    {
        return ChatService::ConnectionState::Connecting;
    }
    
    return ChatService::ConnectionState::NotConnected;
}

QString Odysee::getStateDescription() const
{
    switch (getConnectionState())
    {
    case ConnectionState::NotConnected:
        if (stream.get().isEmpty())
        {
            return tr("Broadcast not specified");
        }

        if (state.streamId.isEmpty())
        {
            return tr("The broadcast is not correct");
        }

        return tr("Not connected");
        
    case ConnectionState::Connecting:
        return tr("Connecting...");
        
    case ConnectionState::Connected:
        return tr("Successfully connected!");

    }

    return "<unknown_state>";
}

void Odysee::reconnectImpl()
{
    socket.close();

    info = Info();

    extractChannelAndVideo(stream.get(), info.channel, info.video);

    if (!info.channel.isEmpty() && !info.video.isEmpty())
    {
        state.streamId = info.channel + "/" + info.video;

        state.streamUrl = "https://odysee.com/@" + info.channel + "/" + info.video;
        state.chatUrl = "https://odysee.com/$/popout/@" + info.channel + "/" + info.video;
    }

    state.controlPanelUrl = "https://odysee.com/$/livestream";

    if (!enabled.get() || state.streamId.isEmpty())
    {
        return;
    }

    requestClaimId();
}

void Odysee::onReceiveWebSocket(const QString &rawData)
{
    if (!enabled.get())
    {
        return;
    }

    checkPingTimer.setInterval(CheckPingSendTimeout);
    checkPingTimer.start();

    const QJsonDocument doc = QJsonDocument::fromJson(rawData.toUtf8());
    const QJsonObject root = doc.object();

    //qDebug() << "received:" << doc;

    const QJsonObject data = root.value("data").toObject();

    const QString type = root.value("type").toString();
    if (type == "delta")
    {
        parseComment(data);
    }
    else if (type == "removed")
    {
        parseRemoved(data);
    }
    else if (type == "livestream")
    {
        // ignore, example:
        // {"data":{"channel_id":"00f95108658709853f623c0c4bca4d8df22bf461","end_time":"0001-01-01T00:00:00Z","live":true,"live_claim_id":"bc4cf0556397ad419f8f2bef6308aa1be7d2f482","live_time":"2023-08-22T23:05:30.692281748Z","protected":false,"release_time":"2023-08-22T23:00:00Z"},"type":"livestream"}
    }
    else if (type == "viewers")
    {
        state.viewers = data.value("connected").toInt();
        emit stateChanged();
    }
    else
    {
        qWarning() << "unknown type" << type << ", received =" << doc;
    }
}

void Odysee::requestClaimId()
{
    if (!enabled.get())
    {
        return;
    }

    const QJsonArray urls = { "lbry://@" + info.channel.replace(':', '#') + "/" + info.video.replace(':', '#') };
    const QJsonObject params = { { "urls", urls } };
    const QByteArray data = QJsonDocument(QJsonObject(
    {
        { "jsonrpc", "2.0" },
        { "method", "resolve" },
        { "params", params }
    })).toJson();

    QNetworkRequest request(QUrl("https://api.na-backend.odysee.com/api/v1/proxy"));
    request.setRawHeader("Content-Type", "application/json-rpc");

    QNetworkReply* reply = network.post(request, data);
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]()
    {
        QByteArray data;
        if (!checkReply(reply, Q_FUNC_INFO, data))
        {
            return;
        }

        const QJsonObject root = QJsonDocument::fromJson(data).object();
        const QJsonObject result = root.value("result").toObject();
        const QStringList keys = result.keys();
        if (keys.isEmpty())
        {
            qWarning() << "keys of result is empty, root =" << root;
            return;
        }

        const QJsonObject videoInfo = result.value(keys.first()).toObject();
        info.claimId = videoInfo.value("claim_id").toString();

        if (info.claimId.isEmpty())
        {
            qWarning() << "failed to find claim id, root =" << root;
            return;
        }

        requestLive();
    });
}

void Odysee::requestLive()
{
    if (!enabled.get())
    {
        return;
    }

    QNetworkRequest request(QUrl("https://api.odysee.live/livestream/is_live"));
    request.setRawHeader("Content-Type", "application/x-www-form-urlencoded");

    const QByteArray data = ("channel_claim_id=" + info.claimId).toUtf8();

    QNetworkReply* reply = network.post(request, data);
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]()
    {
        QByteArray data;
        if (!checkReply(reply, Q_FUNC_INFO, data))
        {
            return;
        }

        const QJsonObject root = QJsonDocument::fromJson(data).object();
        const QJsonObject dataJson = root.value("data").toObject();
        
        state.viewers = dataJson.value("ViewerCount").toInt(-1);
        
        if (state.viewers < 0)
        {
            qWarning() << "failed to get viewers count, root =" << root;
        }

        emit stateChanged();

        if (socket.state() != QAbstractSocket::SocketState::ConnectedState)
        {
            socket.open(
                "wss://sockety.odysee.tv/ws/commentron?id=" +
                info.claimId +
                "&category=@" +
                info.video +
                "&sub_category=commenter");
        }
    });
}

void Odysee::requestChannelInfo(const QString &lbryUrl, const QString& authorId)
{
    if (!enabled.get())
    {
        return;
    }

    const QJsonArray urls = { lbryUrl };
    const QJsonObject params = { { "urls", urls } };
    const QByteArray data = QJsonDocument(QJsonObject(
                                              {
                                                  { "jsonrpc", "2.0" },
                                                  { "method", "resolve" },
                                                  { "params", params }
                                              })).toJson();

    QNetworkRequest request(QUrl("https://api.na-backend.odysee.com/api/v1/proxy"));
    request.setRawHeader("Content-Type", "application/json-rpc");

    QNetworkReply* reply = network.post(request, data);
    QObject::connect(reply, &QNetworkReply::finished, this, [this, authorId, reply]()
    {
        QByteArray raw;
        if (!checkReply(reply, Q_FUNC_INFO, raw))
        {
            return;
        }

        const QJsonObject root = QJsonDocument::fromJson(raw).object();
        const QJsonObject result = root.value("result").toObject();
        const QStringList keys = result.keys();
        if (keys.isEmpty())
        {
            qWarning() << "keys of result is empty, root =" << root;
            return;
        }

        const QJsonObject data = result.value(keys.first()).toObject();

        QUrl avatartUrl = QUrl(data.value("value").toObject()
            .value("thumbnail").toObject()
            .value("url").toString());

        if (avatartUrl.isEmpty())
        {
            avatartUrl = DefaultAvatar;
        }

        avatars.insert(authorId, avatartUrl);

        emit authorDataUpdated(authorId, { { Author::Role::AvatarUrl, QUrl(avatartUrl) } });
    });
}

void Odysee::sendPing()
{
    socket.sendTextMessage("{\"type\":\"ping\"}");
    qWarning() << "not implemented";
}

void Odysee::extractChannelAndVideo(const QString &rawLink, QString &channel, QString &video)
{
    channel = QString();
    video = QString();

    QString link = AxelChat::simplifyUrl(rawLink.trimmed());

    link = AxelChat::removeFromStart(link, "odysee.com", Qt::CaseInsensitive);
    link = AxelChat::removeFromStart(link, "/", Qt::CaseInsensitive);
    link = AxelChat::removeFromStart(link, "@", Qt::CaseInsensitive);

    link = QUrl::fromPercentEncoding(link.toUtf8());

    const QStringList part = link.split('/', Qt::SplitBehaviorFlags::SkipEmptyParts);
    if (part.count() >= 2)
    {
        channel = part[0];
        video = part[1];
    }
}

QString Odysee::extractChannelId(const QString &rawLink)
{
    QString id = rawLink.trimmed();
    id = AxelChat::removeFromStart(id, "lbry://", Qt::CaseInsensitive);

    if (!id.contains('#'))
    {
        qWarning() << "not found '#' in" << rawLink;
        return QString();
    }

    id = id.left(id.indexOf('#') + 2).replace('#', ':');

    if (!id.startsWith('@'))
    {
        id = '@' + id;
    }

    return id;
}

void Odysee::parseComment(const QJsonObject &data)
{
    const QJsonObject comment = data.value("comment").toObject();
    const int64_t timestamp = comment.value("timestamp").toVariant().toLongLong();
    const QString rawText = comment.value("comment").toString();

    const QString lbryUrl = comment.value("channel_url").toString();

    const QString channelId = extractChannelId(lbryUrl);

    const QString authorId = generateAuthorId(comment.value("channel_id").toString());

    const bool isModerator = comment.value("is_moderator").toBool(false);
    const bool isCreator = comment.value("is_creator").toBool(false);
    const bool isFiat = comment.value("is_fiat").toBool(false);

    const double supportAmount = comment.value("support_amount").toDouble(0);
    const QString currency = comment.value("currency").toString().toUpper();

    QUrl avatarUrl;

    if (avatars.contains(authorId))
    {
        avatarUrl = avatars.value(authorId);
    }
    else
    {
        requestChannelInfo(lbryUrl, authorId);
    }

    Author::Builder authorBuilder(
        getServiceType(),
        authorId,
        comment.value("channel_name").toString());

    authorBuilder.setPage("https://odysee.com/" + channelId);

    if (isModerator)
    {
        authorBuilder.addRightBadge("qrc:/resources/images/odysee/badges/moderator.svg");
    }

    if (isCreator)
    {
        authorBuilder.addRightBadge("qrc:/resources/images/odysee/badges/creator.svg");
        authorBuilder.setCustomNicknameBackgroundColor(CreatorNicknameBackgroundColor);
        authorBuilder.setCustomNicknameColor(QColor(255, 255, 255));
    }

    const auto author = authorBuilder.build();

    Message::Builder messageBuilder(
        author,
        generateMessageId(comment.value("comment_id").toString()));

    if (supportAmount > 0)
    {
        Message::TextStyle style;
        style.bold = true;
        messageBuilder.addText(QString("%1 %2\n").arg(supportAmount, 0, 'f', 2).arg(currency), style);

        messageBuilder.setForcedColor(Message::ColorRole::BodyBackgroundColorRole, BackgroundDonationColor);
    }

    messageBuilder
        .addText(rawText)
        .setReceivedTime(QDateTime::currentDateTime())
        .setPublishedTime(QDateTime::fromSecsSinceEpoch(timestamp));

    const auto message = messageBuilder.build();

    emit readyRead({ message}, { author });
}

void Odysee::parseRemoved(const QJsonObject &data)
{
    const QJsonObject comment = data.value("comment").toObject();

    const QString messageId = generateMessageId(comment.value("comment_id").toString());

    const auto pair = Message::Builder::createDeleter(getServiceType(), messageId);

    emit readyRead({ pair.second }, { pair.first });
}
