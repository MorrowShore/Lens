#include "emotesprocessor.h"
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonArray>

namespace
{

static const int EmoteImageHeight = 32;

}

EmotesProcessor::EmotesProcessor(QSettings& settings_, const QString& settingsGroupPathParent_, QNetworkAccessManager& network_, QObject *parent)
    : QObject{parent}
    , settings(settings_)
    , settingsGroupPath(settingsGroupPathParent_ + "/emotes_processor")
    , network(network_)
{
    connect(&timer, &QTimer::timeout, this, [this]()
    {
        if (bttvEmotes.isEmpty())
        {
            loadBttvGlobalEmotes();
        }

        if (ffzEmotes.isEmpty())
        {
            loadFfzGlobalEmotes();
        }
    });
    timer.start(3000);

    loadAll();
}

void EmotesProcessor::processMessage(std::shared_ptr<Message> message)
{
    if (!message)
    {
        qWarning() << Q_FUNC_INFO << "message is null";
        return;
    }

    bool messageUpdated = false;

    const QList<std::shared_ptr<Message::Content>>& prevContents = message->getContents();

    QList<std::shared_ptr<Message::Content>> newContents;
    newContents.reserve(prevContents.count());

    for (const std::shared_ptr<Message::Content>& prevContent : qAsConst(prevContents))
    {
        if (!prevContent)
        {
            continue;
        }

        bool needAddPrevContent = true;

        if (prevContent->getType() == Message::Content::Type::Text)
        {
            if (const Message::Text* prevTextContent = static_cast<const Message::Text*>(prevContent.get()); prevTextContent)
            {
                QString text;

                const QString& prevText = prevTextContent->getText();
                const QStringList words = prevText.split(' ', Qt::KeepEmptyParts);
                for (const QString& word : qAsConst(words))
                {
                    if (const QString url = getEmoteUrl(word); !url.isEmpty())
                    {
                        if (!text.isEmpty())
                        {
                            newContents.push_back(std::make_shared<Message::Text>(text, prevTextContent->getStyle()));
                            text = QString();
                        }

                        newContents.push_back(std::make_shared<Message::Image>(url, EmoteImageHeight, true));

                        messageUpdated = true;
                        needAddPrevContent = false;
                    }
                    else
                    {
                        if (!text.isEmpty())
                        {
                            text += ' ';
                        }

                        text += word;
                    }
                }

                if (!text.isEmpty())
                {
                    newContents.push_back(std::make_shared<Message::Text>(text, prevTextContent->getStyle()));
                    text = QString();
                }
            }
        }

        if (needAddPrevContent)
        {
            newContents.push_back(prevContent);
        }
    }

    if (messageUpdated)
    {
        message->setContents(newContents);
    }
}

void EmotesProcessor::loadAll()
{
    loadBttvGlobalEmotes();
    loadFfzGlobalEmotes();
}

void EmotesProcessor::loadBttvGlobalEmotes()
{
    QNetworkRequest request(QUrl("https://api.betterttv.net/3/cached/emotes/global"));

    QNetworkReply* reply = network.get(request);
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]()
    {
        const QJsonArray array = QJsonDocument::fromJson(reply->readAll()).array();
        reply->deleteLater();

        if (array.isEmpty())
        {
            qWarning() << Q_FUNC_INFO << "array is empty";
            return;
        }

        for (const QJsonValue& v : qAsConst(array))
        {
            const QJsonObject emoteJson = v.toObject();

            const QString id = emoteJson.value("id").toString();
            const QString code = emoteJson.value("code").toString();

            bttvEmotes.insert(code, id);
        }
    });
}

void EmotesProcessor::loadFfzGlobalEmotes()
{
    QNetworkRequest request(QUrl("https://api.frankerfacez.com/v1/set/global"));

    QNetworkReply* reply = network.get(request);
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]()
    {
        const QJsonObject sets = QJsonDocument::fromJson(reply->readAll()).object().value("sets").toObject();
        reply->deleteLater();

        const QStringList keys = sets.keys();
        for (const QString& key : qAsConst(keys))
        {
            parseFfzSet(sets.value(key).toObject());
        }
    });
}

void EmotesProcessor::parseFfzSet(const QJsonObject &set)
{
    const QJsonArray array = set.value("emoticons").toArray();
    for (const QJsonValue& v : qAsConst(array))
    {
        const QJsonObject emoteJson = v.toObject();

        const QString id = QString("%1").arg(emoteJson.value("id").toVariant().toLongLong());
        const QString name = emoteJson.value("name").toString();

        ffzEmotes.insert(name, id);
    }
}

QString EmotesProcessor::getEmoteUrl(const QString &name) const
{
    if (auto it = bttvEmotes.find(name); it != bttvEmotes.end())
    {
        const QString& id = it.value();
        if (id.isEmpty())
        {
            qWarning() << Q_FUNC_INFO << "code is empty for global bttv emote" << name;
        }
        else
        {
            return "https://cdn.betterttv.net/emote/" + id + "/2x.webp";
        }
    }

    if (auto it = ffzEmotes.find(name); it != ffzEmotes.end())
    {
        const QString& id = it.value();
        if (id.isEmpty())
        {
            qWarning() << Q_FUNC_INFO << "code is empty for ffz emote" << name;
        }
        else
        {
            return "https://cdn.frankerfacez.com/emote/" + id + "/2";
        }
    }

    return QString();
}
