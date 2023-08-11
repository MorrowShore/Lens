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
    });
    timer.start(3000);

    loadBttvGlobalEmotes();
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

QString EmotesProcessor::getEmoteUrl(const QString &name) const
{
    if (auto it = bttvEmotes.find(name); it != bttvEmotes.end())
    {
        const QString& id = it.value();
        if (id.isEmpty())
        {
            qWarning() << Q_FUNC_INFO << "code is empty for emote" << name;
        }
        else
        {
            return "https://cdn.betterttv.net/emote/" + id + "/2x.webp";
        }
    }

    return QString();
}
