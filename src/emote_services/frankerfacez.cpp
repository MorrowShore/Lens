#include "frankerfacez.h"
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

FrankerFaceZ::FrankerFaceZ(QNetworkAccessManager& network_, QObject *parent)
    : EmoteService{parent}
    , network(network_)
{

}

QString FrankerFaceZ::findEmoteUrl(const QString &name) const
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

void FrankerFaceZ::loadGlobal()
{
    loadedGlobal = false;

    QNetworkRequest request(QUrl("https://api.frankerfacez.com/v1/set/global"));

    QNetworkReply* reply = network.get(request);
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]()
    {
        loadedGlobal = true;

        const QJsonObject sets = QJsonDocument::fromJson(reply->readAll()).object().value("sets").toObject();
        reply->deleteLater();

        const QStringList keys = sets.keys();
        for (const QString& key : qAsConst(keys))
        {
            global.insert(parseSet(sets.value(key).toObject()));
        }
    });
}

void FrankerFaceZ::loadChannel(const QString &id)
{
    loadedChannel = false;

    {
        QNetworkRequest request(QUrl("https://api.frankerfacez.com/v1/room/id/" + id));

        QNetworkReply* reply = network.get(request);
        QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]()
        {
            loadedChannel = true;

            const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
            const QJsonObject sets = root.value("sets").toObject();
            reply->deleteLater();

            const QStringList keys = sets.keys();
            for (const QString& key : qAsConst(keys))
            {
                channel.insert(parseSet(sets.value(key).toObject()));
            }
        });
    }

    {
        QNetworkRequest request(QUrl("https://api.frankerfacez.com/v1/user/id/" + id));

        QNetworkReply* reply = network.get(request);
        QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]()
        {
            loadedChannel = true;

            const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
            const QJsonObject sets = root.value("sets").toObject();
            reply->deleteLater();

            const QStringList keys = sets.keys();
            for (const QString& key : qAsConst(keys))
            {
                channel.insert(parseSet(sets.value(key).toObject()));
            }
        });
    }
}

QHash<QString, QString> FrankerFaceZ::parseSet(const QJsonObject &set)
{
    QHash<QString, QString> result;

    const QJsonArray array = set.value("emoticons").toArray();
    for (const QJsonValue& v : qAsConst(array))
    {
        const QJsonObject emoteJson = v.toObject();

        const QString id = QString("%1").arg(emoteJson.value("id").toVariant().toLongLong());
        const QString name = emoteJson.value("name").toString();

        const QString url = "https://cdn.frankerfacez.com/emote/" + id + "/2";

        result.insert(name, url);
    }

    return result;
}
