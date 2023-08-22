#include "odysee.h"
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

namespace
{

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
    reconnect();
}

ChatService::ConnectionStateType Odysee::getConnectionStateType() const
{
    if (state.connected)
    {
        return ChatService::ConnectionStateType::Connected;
    }
    else if (enabled.get() && !state.streamId.isEmpty())
    {
        return ChatService::ConnectionStateType::Connecting;
    }

    return ChatService::ConnectionStateType::NotConnected;
}

QString Odysee::getStateDescription() const
{
    switch (getConnectionStateType())
    {
    case ConnectionStateType::NotConnected:
        if (stream.get().isEmpty())
        {
            return tr("Channel not specified");
        }

        if (state.streamId.isEmpty())
        {
            return tr("The channel is not correct");
        }

        return tr("Not connected");

    case ConnectionStateType::Connecting:
        return tr("Connecting...");

    case ConnectionStateType::Connected:
        return tr("Successfully connected!");

    }

    return "<unknown_state>";
}

void Odysee::reconnectImpl()
{
    state = State();
    info = Info();

    if (!enabled.get())
    {
        return;
    }

    requestClaimId();
}

void Odysee::requestClaimId()
{
    if (!enabled.get())
    {
        return;
    }

    const QJsonArray urls = { "lbry://@AxelChatDev#3/teststream_ad230j034f#5" }; //TODO
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
            qWarning() << Q_FUNC_INFO << "keys of result is empty, root =" << root;
            return;
        }

        const QJsonObject videoInfo = result.value(keys.first()).toObject();
        info.claimId = videoInfo.value("claim_id").toString();

        if (info.claimId.isEmpty())
        {
            qWarning() << Q_FUNC_INFO << "failed to find claim id, root =" << root;
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

        state.viewersCount = dataJson.value("ViewerCount").toInt(-1);

        if (state.viewersCount < 0)
        {
            qWarning() << Q_FUNC_INFO << "failed to get viewers count, root =" << root;
        }

        emit stateChanged();
    });
}
