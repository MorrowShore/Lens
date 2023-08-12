#include "betterttv.h"
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

BetterTTV::BetterTTV(QNetworkAccessManager& network_, QObject *parent)
    : EmoteService{parent}
    , network(network_)
{

}

QString BetterTTV::findEmoteUrl(const QString &name) const
{
    if (auto it = global.find(name); it != global.end())
    {
        const QString& url = it.value();
        if (url.isEmpty())
        {
            qWarning() << Q_FUNC_INFO << "name is empty for global emote" << name;
        }
        else
        {
            return url;
        }
    }

    if (auto it = channel.find(name); it != channel.end())
    {
        const QString& url = it.value();
        if (url.isEmpty())
        {
            qWarning() << Q_FUNC_INFO << "name is empty for global emote" << name;
        }
        else
        {
            return url;
        }
    }

    return QString();
}

void BetterTTV::loadGlobal()
{
    loadedGlobal = false;

    QNetworkRequest request(QUrl("https://api.betterttv.net/3/cached/emotes/global"));

    QNetworkReply* reply = network.get(request);
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]()
    {
        loadedGlobal = true;

        const QJsonArray array = QJsonDocument::fromJson(reply->readAll()).array();
        reply->deleteLater();

        if (array.isEmpty())
        {
            qWarning() << Q_FUNC_INFO << "array is empty";
            return;
        }

        global.insert(parseArray(array));
    });
}

void BetterTTV::loadChannel(const QString &id)
{
    loadedChannel = false;

    QNetworkRequest request(QUrl("https://api.betterttv.net/3/cached/users/twitch/" + id));

    QNetworkReply* reply = network.get(request);
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]()
    {
        loadedChannel = true;

        const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
        reply->deleteLater();

        channel.insert(parseArray(root.value("sharedEmotes").toArray()));
        channel.insert(parseArray(root.value("channelEmotes").toArray()));
    });
}

QHash<QString, QString> BetterTTV::parseArray(const QJsonArray &array)
{
    QHash<QString, QString> result;

    for (const QJsonValue& v : qAsConst(array))
    {
        const QJsonObject emoteJson = v.toObject();

        const QString id = emoteJson.value("id").toString();
        const QString name = emoteJson.value("code").toString();

        const QString url = "https://cdn.betterttv.net/emote/" + id + "/2x.webp";

        result.insert(name, url);
    }

    return result;
}
