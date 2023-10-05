#include "chathandler.h"
#include "chat_services/youtubehtml.h"
#include "chat_services/youtubebrowser.h"
#include "chat_services/twitch.h"
#include "chat_services/trovo.h"
#include "chat_services/rumble.h"
#include "chat_services/goodgame.h"
#include "chat_services/vkplaylive.h"
#include "chat_services/telegram.h"
#include "chat_services/discord/discord.h"
#include "chat_services/vkvideo.h"
#include "chat_services/dlive.h"
#include "chat_services/wasd.h"
#include "chat_services/kick.h"
#include "chat_services/odysee.h"
#include "chat_services/donationalerts.h"
#include "chat_services/donatepay.h"
#include "models/author.h"
#include "models/message.h"
#include <QCoreApplication>
#ifndef AXELCHAT_LIBRARY
#include <QDesktopServices>
#endif
#include <QDebug>

namespace
{

static const QString SettingsGroupPath = "chat_handler";

static const QString SettingsEnabledSoundNewMessage             = SettingsGroupPath + "/enabledSoundNewMessage";
static const QString SettingsEnabledShowAuthorNameChanged       = SettingsGroupPath + "/enabledShowAuthorNameChanged";
static const QString SettingsProxyEnabled                       = SettingsGroupPath + "/proxyEnabled";
static const QString SettingsProxyAddress                       = SettingsGroupPath + "/proxyServerAddress";
static const QString SettingsProxyPort                          = SettingsGroupPath + "/proxyServerPort";

static const QList<QColor> GeneratingColors =
{
    QColor(255, 30,  30 ),
    QColor(141, 0,   206),
    QColor(60,  60,  255),
    QColor(141, 143, 244),
    QColor(60,  143, 244),
    QColor(191, 55,  206),
    QColor(255, 107, 161),
    QColor(119, 204, 249),
    QColor(157, 0,   157),
    QColor(194, 0,   105),
    QColor(255, 97,  12 ),
    QColor(255, 221, 0  ),
    QColor(0,   255, 144),
    QColor(0,   255, 255),
    QColor(61,  201, 61 ),
    QColor(224, 0,   78 ),
};

}

ChatHandler::ChatHandler(QSettings& settings_, QNetworkAccessManager& network_, cweqt::Manager& web_, QObject *parent)
    : QObject(parent)
    , settings(settings_)
    , network(network_)
    , web(web_)
    , emotesProcessor(settings_, SettingsGroupPath, network_)
    , outputToFile(settings, SettingsGroupPath + "/output_to_file", network, messagesModel, services)
    , bot(settings, SettingsGroupPath + "/chat_bot")
    , authorQMLProvider(*this, messagesModel, outputToFile)
    , webSocket(*this)
    , tcpServer(services)
{
    connect(&outputToFile, &OutputToFile::authorNameChanged, this, &ChatHandler::onAuthorNameChanged);

    setEnabledSoundNewMessage(settings.value(SettingsEnabledSoundNewMessage, _enabledSoundNewMessage).toBool());

    setEnabledShowAuthorNameChanged(settings.value(SettingsEnabledShowAuthorNameChanged, _enableShowAuthorNameChanged).toBool());

    setProxyEnabled(settings.value(SettingsProxyEnabled, _enabledProxy).toBool());
    setProxyServerAddress(settings.value(SettingsProxyAddress, _proxy.hostName()).toString());
    setProxyServerPort(settings.value(SettingsProxyPort, _proxy.port()).toInt());
    
    addService<YouTubeHtml>();
    //addService<YouTubeBrowser>();
    addService<Twitch>();
    addService<Trovo>();
    addService<Kick>();
    addService<Rumble>();
    addService<DLive>();
    addService<Odysee>();
    addService<GoodGame>();
    addService<VkPlayLive>();
    addService<VkVideo>();
    addService<Wasd>();
    addService<Telegram>();
    addService<Discord>();
    addService<DonationAlerts>();
    //addService<DonatePay>();

    QTimer::singleShot(2000, [this]()
    {
        Q_UNUSED(this)
        //addTestMessages();
    });
}

ChatHandler::~ChatHandler()
{
    const int count = services.count();
    for (int i = 0; i < count; i++)
    {
        removeService(0);
    }
}

void ChatHandler::onReadyRead(const QList<std::shared_ptr<Message>>& messages, const QList<std::shared_ptr<Author>>& authors)
{
    ChatService* service = static_cast<ChatService*>(sender());

    if (messages.isEmpty() && authors.isEmpty())
    {
        return;
    }

    if (messages.count() != authors.count())
    {
        qWarning() << "The number of messages is not equal to the number of authors";
        return;
    }

    const AxelChat::ServiceType serviceType = service ? service->getServiceType() : AxelChat::ServiceType::Unknown;

    QList<std::shared_ptr<Message>> messagesValidToAdd;
    QList<std::shared_ptr<Author>> updatedAuthors;

    for (int i = 0; i < messages.count(); ++i)
    {
        std::shared_ptr<Message> message = messages[i];
        if (!message)
        {
            qWarning() << "message is null";
            continue;
        }

        if (messagesModel.contains(message->getId()) && !message->isHasFlag(Message::Flag::DeleterItem))
        {
            continue;
        }

        std::shared_ptr<Author> author = authors[i];
        if (!author)
        {
            qWarning() << "author is null";
            continue;
        }

        //author->setValue(Author::Role::CustomNicknameColor, AxelChat::generateColor(author->getId(), GeneratingColors));

        messagesModel.addAuthor(author);

        if ((service && service->isEnabledThirdPartyEmotes()) || !service)
        {
            emotesProcessor.processMessage(message);
        }

        messagesValidToAdd.append(message);
    }

    outputToFile.writeAuthors(updatedAuthors);

    if (messagesValidToAdd.isEmpty())
    {
        return;
    }

    for (int i = 0; i < messagesValidToAdd.count(); ++i)
    {
        const std::shared_ptr<Message>& message = messagesValidToAdd[i];

#ifndef AXELCHAT_LIBRARY
        if (!message->isHasFlag(Message::Flag::IgnoreBotCommand))
        {
            bot.processMessage(message);
        }
#endif
        
        messagesModel.addMessage(message);
    }

    outputToFile.writeMessages(messagesValidToAdd, serviceType);
    webSocket.sendMessages(messagesValidToAdd);

    emit messagesDataChanged();

    if (_enabledSoundNewMessage && !messages.empty())
    {
        playNewMessageSound();
    }
}

void ChatHandler::sendTestMessage(const QString &text)
{
    std::shared_ptr<Author> author = Author::getSoftwareAuthor();

    std::shared_ptr<Message> message = std::make_shared<Message>(
        QList<std::shared_ptr<Message::Content>>{std::make_shared<Message::Text>(text)},
        author);

    message->setCustomAuthorName(tr("Test Message"));
    message->setCustomAuthorAvatarUrl(QUrl("qrc:/resources/images/flask2.svg"));

    onReadyRead({message}, {author});
}

void ChatHandler::sendSoftwareMessage(const QString &text)
{
    std::shared_ptr<Author> author = Author::getSoftwareAuthor();
\
    std::shared_ptr<Message> message = std::make_shared<Message>(
        QList<std::shared_ptr<Message::Content>>{std::make_shared<Message::Text>(text)},
        author,
        QDateTime::currentDateTime(),
        QDateTime::currentDateTime(),
        QString());

    onReadyRead({message}, {author});
}

void ChatHandler::playNewMessageSound()
{
#ifdef QT_MULTIMEDIA_LIB
    if (_soundDefaultNewMessage)
    {
        _soundDefaultNewMessage->play();
    }
    else
    {
        qDebug() << "sound not exists";
    }
#else
    qWarning() << "module multimedia not included";
#endif
}

void ChatHandler::onAuthorDataUpdated(const QString& authorId, const QMap<Author::Role, QVariant>& values)
{
    AxelChat::ServiceType serviceType = AxelChat::ServiceType::Unknown;
    ChatService* service = qobject_cast<ChatService*>(sender());
    if (service)
    {
        serviceType = service->getServiceType();
    }

    const QList<Author::Role> roles = values.keys();
    for (const Author::Role role : roles)
    {
        if (role == Author::Role::AvatarUrl)
        {
            outputToFile.downloadAvatar(authorId, serviceType, values[role].toUrl());
        }
    }

    messagesModel.setAuthorValues(serviceType, authorId, values);
    webSocket.sendAuthorValues(authorId, values);
}

void ChatHandler::clearMessages()
{
    messagesModel.clear();
}

void ChatHandler::onStateChanged()
{
    ChatService* service = dynamic_cast<ChatService*>(sender());
    if (service)
    {
        outputToFile.writeServiceState(service);
    }

    outputToFile.writeApplicationState(true, getViewersTotalCount());
    webSocket.sendState();

    emit connectedCountChanged();
    emit viewersTotalCountChanged();
}

#ifndef AXELCHAT_LIBRARY
void ChatHandler::openProgramFolder()
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(QString("file:///") + QCoreApplication::applicationDirPath()));
}
#endif

void ChatHandler::onAuthorNameChanged(const Author& author, const QString &prevName, const QString &newName)
{
    if (_enableShowAuthorNameChanged)
    {
        sendSoftwareMessage(tr("%1: \"%2\" changed name to \"%3\"")
                            .arg(ChatService::getName(author.getServiceType()), prevName, newName));
    }
}

void ChatHandler::updateProxy()
{
    if (_enabledProxy)
    {
        network.setProxy(_proxy);
    }
    else
    {
        network.setProxy(QNetworkProxy(QNetworkProxy::NoProxy));
    }

    for (const std::shared_ptr<ChatService>& service : qAsConst(services))
    {
        if (!service)
        {
            qWarning() << "service is null";
            continue;
        }

        service->reconnect();
    }

    emit proxyChanged();
}

void ChatHandler::removeService(const int index)
{
    if (index >= services.count() || index < 0)
    {
        qWarning() << "index not valid";
        return;
    }

    std::shared_ptr<ChatService> service = services.at(index);
    if (service)
    {
        QObject::disconnect(service.get(), nullptr, this, nullptr);
        QObject::disconnect(this, nullptr, service.get(), nullptr);
    }

    services.removeAt(index);
}

template<typename ChatServiceInheritedClass>
void ChatHandler::addService()
{
    static_assert(std::is_base_of<ChatService, ChatServiceInheritedClass>::value, "ChatServiceInheritedClass must derive from ChatService");

    std::shared_ptr<ChatServiceInheritedClass> service = std::make_shared<ChatServiceInheritedClass>(settings, SettingsGroupPath, network, web, this);

    QObject::connect(service.get(), &ChatService::stateChanged, this, &ChatHandler::onStateChanged);
    QObject::connect(service.get(), &ChatService::readyRead, this, &ChatHandler::onReadyRead);
    QObject::connect(service.get(), &ChatService::authorDataUpdated, this, &ChatHandler::onAuthorDataUpdated);

    services.append(service);

    if (service->getServiceType() == AxelChat::ServiceType::Twitch)
    {
        if (std::shared_ptr<Twitch> twitch = std::dynamic_pointer_cast<Twitch>(service); twitch)
        {
            emotesProcessor.connectTwitch(twitch);
        }
        else
        {
            qWarning() << "Failed to cast to Twitch";
        }
    }
}

void ChatHandler::addTestMessages()
{
    QList<std::shared_ptr<Message>> messages;
    QList<std::shared_ptr<Author>> authors;

    sendSoftwareMessage("Hello, world!");

    sendTestMessage("Hello developer!");

    {
        auto author = Author::Builder(
                          AxelChat::ServiceType::YouTube,
                          QUuid::createUuid().toString(),
                          "Mario")
                          .setAvatar("https://static.wikia.nocookie.net/mario/images/e/e3/MPS_Mario.png/revision/latest/scale-to-width-down/350?cb=20220814154953")
                          .addLeftTag(Author::Tag("youtuber", QColor(255, 0, 0), QColor(255, 255, 255)))
                          .build();

        messages.append(
            Message::Builder(author)
            .addText("Hello it's me Mario!")
            .build());

        authors.append(author);
    }

    {
        auto author = Author::Builder(
                          AxelChat::ServiceType::Twitch,
                          QUuid::createUuid().toString(),
                          "Big Smoke")
                          .setAvatar("https://static.wikia.nocookie.net/gtawiki/images/b/bf/BigSmoke-GTASAde.png/revision/latest/scale-to-width-down/350?cb=20211113214309")
                          .build();

        messages.append(
            Message::Builder(author)
                .addText("I’ll have two number 9s, a number 9 large, a number 6 with extra dip, a number 7, two number 45s, one with cheese, and a large soda")
                .build());

        authors.append(author);
    }

    {
        auto author = Author::Builder(
                          AxelChat::ServiceType::Trovo,
                          QUuid::createUuid().toString(),
                          "Luigi")
                          .setAvatar("https://static.wikia.nocookie.net/mario/images/7/72/MPSS_Luigi.png/revision/latest/scale-to-width-down/254?cb=20220705200355")
                          .setCustomNicknameColor(QColor("#FF00C7"))
                          .setCustomNicknameBackgroundColor(QColor("#FFFFFF"))
                          .build();

        messages.append(
            Message::Builder(author)
                .addText("Mamma mia!")
                .setForcedColor(Message::ColorRole::BodyBackgroundColorRole, QColor("#A8D9FF"))
                .setDestination({ "Underground" })
                .build());

        authors.append(author);
    }

    {
        auto author = Author::Builder(
                          AxelChat::ServiceType::Kick,
                          QUuid::createUuid().toString(),
                          "Sonic")
                          .setAvatar("https://static.wikia.nocookie.net/sonic/images/5/57/Sonic_Superstars_Sonic.png/revision/latest/smart/width/53/height/53?cb=20230801191457")
                          .build();

        messages.append(
            Message::Builder(author)
                .addText("Don't slow down!")
                .build());

        authors.append(author);
    }

    {
        auto author = Author::Builder(
                          AxelChat::ServiceType::Rumble,
                          QUuid::createUuid().toString(),
                          "Battle Kid")
                          .setAvatar("https://static.wikia.nocookie.net/gamegrumps/images/6/67/Battle_Kid_Fortress_of_Peril.png/revision/latest?cb=20141019030833")
                          .setCustomNicknameColor(QColor(133, 199, 66))
                          .build();

        messages.append(
            Message::Builder(author)
                .addText("The evil boss has been taken down!")
                .build());

        authors.append(author);
    }

    {
        auto author = Author::Builder(
                          AxelChat::ServiceType::DLive,
                          QUuid::createUuid().toString(),
                          "Knuckles")
                          .setAvatar("https://static.wikia.nocookie.net/sega/images/6/6e/Knuckles_the_Echidna_Sonic_Frontiers.webp/revision/latest?cb=20221108144328")
                          .build();

        messages.append(
            Message::Builder(author)
                .addText("Dr. Robotnik, I trusted you")
                .build());

        authors.append(author);
    }

    {
        auto author = Author::Builder(
                          AxelChat::ServiceType::Odysee,
                          QUuid::createUuid().toString(),
                          "Commander Keen")
                          .setAvatar("https://static.wikia.nocookie.net/p__/images/c/c5/248240-ck_goodbye_large.gif/revision/latest?cb=20161124032446&path-prefix=protagonist")
                          .setCustomNicknameColor(QColor(255, 140, 0))
                          .build();

        messages.append(
            Message::Builder(author)
                .addText("Let's go!")
                .build());

        authors.append(author);
    }

    {
        auto author = Author::Builder(
                          AxelChat::ServiceType::GoodGame,
                          QUuid::createUuid().toString(),
                          "CJ")
                          .setAvatar("https://static.wikia.nocookie.net/gtawiki/images/2/29/CarlJohnson-GTASAde-Infobox.png/revision/latest/scale-to-width-down/350?cb=20211113054252")
                          .addLeftBadge("https://static.wikia.nocookie.net/gtawiki/images/2/29/CarlJohnson-GTASAde-Infobox.png/revision/latest/scale-to-width-down/350?cb=20211113054252")
                          .addLeftBadge("https://static.wikia.nocookie.net/gtawiki/images/2/29/CarlJohnson-GTASAde-Infobox.png/revision/latest/scale-to-width-down/350?cb=20211113054252")
                          .addRightBadge("https://static.wikia.nocookie.net/gtawiki/images/2/29/CarlJohnson-GTASAde-Infobox.png/revision/latest/scale-to-width-down/350?cb=20211113054252")
                          .addRightBadge("https://static.wikia.nocookie.net/gtawiki/images/2/29/CarlJohnson-GTASAde-Infobox.png/revision/latest/scale-to-width-down/350?cb=20211113054252")
                          .build();

        messages.append(
            Message::Builder(author)
                .addText("Here We Go Again")
                .build());

        authors.append(author);
    }

    {
        auto author = Author::Builder(
                          AxelChat::ServiceType::VkPlayLive,
                          QUuid::createUuid().toString(),
                          "Kenneth Rosenberg")
                          .setAvatar("https://static.wikia.nocookie.net/p__/images/b/b2/Ken_rosenberg.jpg/revision/latest?cb=20130915190559&path-prefix=protagonist")
                          .build();

        messages.append(
            Message::Builder(author)
                .addText("Hey, just like old times, huh, Tommy?")
                .build());

        authors.append(author);
    }

    {
        auto author = Author::Builder(
                          AxelChat::ServiceType::VkVideo,
                          QUuid::createUuid().toString(),
                          "Axel Stone")
                          .setAvatar("https://static.wikia.nocookie.net/streetsofrage/images/6/60/PXZ2-Axel_Stone.png/revision/latest?cb=20190829173626")
                          .setCustomNicknameColor(QColor(255, 255, 0))
                          .build();

        messages.append(
            Message::Builder(author)
                .addText("My fists can shatter any obstacle in my path!")
                .build());

        authors.append(author);
    }

    {
        auto author = Author::Builder(
                          AxelChat::ServiceType::Wasd,
                          QUuid::createUuid().toString(),
                          "Doom Guy")
                          .setAvatar("https://static.wikia.nocookie.net/doom/images/3/30/Doomguyface.jpg/revision/latest?cb=20110328073223")
                          .setCustomNicknameColor(QColor(255, 0, 0))
                          .build();

        messages.append(
            Message::Builder(author)
                .addImage(QUrl("https://static.wikia.nocookie.net/doom/images/b/b4/Shotgun_prev.png/revision/latest?cb=20200318133934"), 40)
                .build());

        authors.append(author);
    }

    {
        auto author = Author::Builder(
                          AxelChat::ServiceType::Telegram,
                          QUuid::createUuid().toString(),
                          "G-Man")
                          .setAvatar("https://static.wikia.nocookie.net/half-life/images/4/41/G-Man_Alyx_Trailer.jpg/revision/latest/scale-to-width-down/350?cb=20191122020607&path-prefix=en")
                          .build();

        messages.append(
            Message::Builder(author)
                .addText("Rise and shine, Mister Freeman")
                .build());

        authors.append(author);
    }

    {
        auto author = Author::Builder(
                          AxelChat::ServiceType::Discord,
                          QUuid::createUuid().toString(),
                          "Gordon Freeman")
                          .setAvatar("https://static.wikia.nocookie.net/half-life/images/1/1f/GordonALYX.png/revision/latest/scale-to-width-down/350?cb=20220520125500&path-prefix=en")
                          .build();

        messages.append(
            Message::Builder(author)
                .setDestination({ "Xen", "Stasis" })
                .addText("\U0001F914")
                .build());

        authors.append(author);
    }

    {
        auto author = Author::Builder(
                          AxelChat::ServiceType::DonationAlerts,
                          QUuid::createUuid().toString(),
                          "S.D.")
                          .setAvatar("https://melmagazine.com/wp-content/uploads/2022/07/ggAsDmL-1-1024x726.jpeg")
                          .setCustomNicknameColor(QColor(245, 144, 7))
                          .build();

        messages.append(
            Message::Builder(author)
                .addText("1 USD\n", Message::TextStyle(true, false))
                .addText("I’d buy that for a dollar!")
                .setForcedColor(Message::ColorRole::BodyBackgroundColorRole, QColor(255, 209, 147))
                .setFlag(Message::Flag::DonateWithText)
                .build());

        authors.append(author);
    }

    {
        auto author = Author::Builder(
                          AxelChat::ServiceType::DonatePayRu,
                          QUuid::createUuid().toString(),
                          "Scrooge McDuck")
                          .setAvatar("https://static.wikia.nocookie.net/disney/images/3/38/Scrooge_%28Mickey_Mouse_2013%29.jpeg/revision/latest/scale-to-width-down/1000?cb=20150114140251")
                          .setCustomNicknameColor(QColor(68, 171, 79))
                          .build();

        messages.append(
            Message::Builder(author)
                .addText("1000000 USD\n", Message::TextStyle(true, false))
                .addText("A modest donation to my favorite streamer")
                .setForcedColor(Message::ColorRole::BodyBackgroundColorRole, QColor(255, 209, 147))
                .setFlag(Message::Flag::DonateWithText)
                .build());

        authors.append(author);
    }

    onReadyRead(messages, authors);
}

#ifndef AXELCHAT_LIBRARY
ChatBot& ChatHandler::getBot()
{
    return bot;
}

OutputToFile& ChatHandler::getOutputToFile()
{
    return outputToFile;
}
#endif

MessagesModel& ChatHandler::getMessagesModel()
{
    return messagesModel;
}

void ChatHandler::setEnabledSoundNewMessage(bool enabled)
{
    if (_enabledSoundNewMessage != enabled)
    {
        _enabledSoundNewMessage = enabled;

        settings.setValue(SettingsEnabledSoundNewMessage, enabled);

        emit enabledSoundNewMessageChanged();
    }
}

void ChatHandler::setEnabledShowAuthorNameChanged(bool enabled)
{
    if (_enableShowAuthorNameChanged != enabled)
    {
        _enableShowAuthorNameChanged = enabled;

        settings.setValue(SettingsEnabledShowAuthorNameChanged, enabled);

        emit enabledShowAuthorNameChangedChanged();
    }
}

int ChatHandler::connectedCount() const
{
    int result = 0;

    for (const std::shared_ptr<ChatService>& service : qAsConst(services))
    {
        if (!service)
        {
            qWarning() << "service is null";
            continue;
        }

        if (service->getConnectionState()  == ChatService::ConnectionStateType::Connected)
        {
            result++;
        }
    }

    return result;
}

int ChatHandler::getViewersTotalCount() const
{
    int result = 0;

    for (const std::shared_ptr<ChatService>& service : qAsConst(services))
    {
        if (!service)
        {
            qWarning() << "service is null";
            continue;
        }

        const int count = service->getViewersCount();
        if (count >= 0)
        {
            result += count;
        }
    }

    return result;
}

bool ChatHandler::isKnownViewesServicesMoreOne() const
{
    int count = 0;

    for (const std::shared_ptr<ChatService>& service : qAsConst(services))
    {
        if (!service)
        {
            continue;
        }
        
        if (service->getState().viewers >= 0)
        {
            count++;

            if (count >= 2)
            {
                return true;
            }
        }
    }

    return false;
}

void ChatHandler::setProxyEnabled(bool enabled)
{
    if (_enabledProxy != enabled)
    {
        _enabledProxy = enabled;

        settings.setValue(SettingsProxyEnabled, enabled);

        updateProxy();
    }
}

void ChatHandler::setProxyServerAddress(QString address)
{
    address = address.trimmed();

    if (_proxy.hostName() != address)
    {
        _proxy.setHostName(address);

        settings.setValue(SettingsProxyAddress, address);

        updateProxy();
    }
}

void ChatHandler::setProxyServerPort(int port)
{
    if (_proxy.port() != port)
    {
        _proxy.setPort(port);

        settings.setValue(SettingsProxyPort, port);

        updateProxy();
    }
}

QNetworkProxy ChatHandler::proxy() const
{
    if (_enabledProxy)
    {
        return _proxy;
    }

    return QNetworkProxy(QNetworkProxy::NoProxy);
}

int ChatHandler::getServicesCount() const
{
    return services.count();
}

ChatService *ChatHandler::getServiceAtIndex(int index) const
{
    if (index >= services.count() || index < 0)
    {
        return nullptr;
    }

    return services.at(index).get();
}

ChatService *ChatHandler::getServiceByType(int type) const
{
    for (const std::shared_ptr<ChatService>& service : services)
    {
        if (!service)
        {
            qWarning() << "service is null";
            continue;
        }

        if (service->getServiceType() == (AxelChat::ServiceType)type)
        {
            return service.get();
        }
    }

    return nullptr;
}

QUrl ChatHandler::getServiceIconUrl(int serviceType) const
{
    return ChatService::getIconUrl((AxelChat::ServiceType)serviceType);
}

QUrl ChatHandler::getServiceName(int serviceType) const
{
    return ChatService::getName((AxelChat::ServiceType)serviceType);
}
