#include "emotesprocessor.h"
#include "emote_services/betterttv.h"
#include "emote_services/frankerfacez.h"
#include "emote_services/seventv.h"
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
        for (const std::shared_ptr<EmoteService>& service : qAsConst(services))
        {
            if (!service)
            {
                qWarning() << Q_FUNC_INFO << "service is null";
                continue;
            }

            if (!service->isLoadedGlobal())
            {
                service->loadGlobal();
            }

            if (!service->isLoadedChannel() && twitchChannelInfo)
            {
                service->loadChannel(twitchChannelInfo->id);
            }
        }
    });
    timer.start(3000);

    addService<BetterTTV>();
    addService<FrankerFaceZ>();
    addService<SevenTV>();

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

void EmotesProcessor::connectTwitch(std::shared_ptr<Twitch> twitch)
{
    connect(twitch.get(), &Twitch::channelInfoChanged, this, [this, twitch]()
    {
        twitchChannelInfo = twitch->getChannelInfo();
        loadAll();
    });
}

void EmotesProcessor::loadAll()
{
    for (const std::shared_ptr<EmoteService>& service : qAsConst(services))
    {
        if (!service)
        {
            qWarning() << Q_FUNC_INFO << "service is null";
            continue;
        }

        service->loadGlobal();

        if (twitchChannelInfo)
        {
            service->loadChannel(twitchChannelInfo->id);
        }
    }
}

QString EmotesProcessor::getEmoteUrl(const QString &name) const
{
    for (const std::shared_ptr<EmoteService>& service : qAsConst(services))
    {
        if (!service)
        {
            qWarning() << Q_FUNC_INFO << "service is null";
            continue;
        }

        if (const QString url = service->findEmoteUrl(name); !url.isEmpty())
        {
            return url;
        }
    }

    return QString();
}

template<typename EmoteServiceInheritedClass>
void EmotesProcessor::addService()
{
    static_assert(std::is_base_of<EmoteService, EmoteServiceInheritedClass>::value, "EmoteServiceInheritedClass must derive from EmoteService");

    std::shared_ptr<EmoteServiceInheritedClass> service = std::make_shared<EmoteServiceInheritedClass>(network, this);

    services.append(service);
}
