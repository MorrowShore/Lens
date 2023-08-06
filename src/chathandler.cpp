#include "chathandler.h"
#include "chat_services/youtube.h"
#include "chat_services/twitch.h"
#include "chat_services/trovo.h"
#include "chat_services/rumble.h"
#include "chat_services/goodgame.h"
#include "chat_services/vkplaylive.h"
#include "chat_services/telegram.h"
#include "chat_services/discord.h"
#include "chat_services/vkvideo.h"
#include "chat_services/wasd.h"
#include "chat_services/kick.h"
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
static const QString SettingsEnabledClearMessagesOnLinkChange   = SettingsGroupPath + "/enabledClearMessagesOnLinkChange";
static const QString SettingsEnabledShowAuthorNameChanged       = SettingsGroupPath + "/enabledShowAuthorNameChanged";
static const QString SettingsProxyEnabled                       = SettingsGroupPath + "/proxyEnabled";
static const QString SettingsProxyAddress                       = SettingsGroupPath + "/proxyServerAddress";
static const QString SettingsProxyPort                          = SettingsGroupPath + "/proxyServerPort";

}

ChatHandler::ChatHandler(QSettings& settings_, QNetworkAccessManager& network_, cweqt::Manager& web_, QObject *parent)
    : QObject(parent)
    , settings(settings_)
    , network(network_)
    , web(web_)
    , outputToFile(settings, SettingsGroupPath + "/output_to_file", network, messagesModel, services)
    , bot(settings, SettingsGroupPath + "/chat_bot")
    , authorQMLProvider(*this, messagesModel, outputToFile)
    , webSocket(*this)
    , tcpServer(services)
{
    connect(&outputToFile, &OutputToFile::authorNameChanged, this, &ChatHandler::onAuthorNameChanged);

    setEnabledSoundNewMessage(settings.value(SettingsEnabledSoundNewMessage, _enabledSoundNewMessage).toBool());

    setEnabledClearMessagesOnLinkChange(settings.value(SettingsEnabledClearMessagesOnLinkChange, _enabledClearMessagesOnLinkChange).toBool());

    setEnabledShowAuthorNameChanged(settings.value(SettingsEnabledShowAuthorNameChanged, _enableShowAuthorNameChanged).toBool());

    setProxyEnabled(settings.value(SettingsProxyEnabled, _enabledProxy).toBool());
    setProxyServerAddress(settings.value(SettingsProxyAddress, _proxy.hostName()).toString());
    setProxyServerPort(settings.value(SettingsProxyPort, _proxy.port()).toInt());

    addService<YouTube>();
    addService<Twitch>();
    addService<Trovo>();
    addService<Rumble>();
    addService<Kick>();
    addService<GoodGame>();
    addService<VkPlayLive>();
    addService<VkVideo>();
    addService<Wasd>();
    addService<Telegram>();
    addService<Discord>();

    QTimer::singleShot(2000, [this]()
    {
        Q_UNUSED(this)
        //addTestMessages();
    });
}

void ChatHandler::onReadyRead(QList<Message>& messages, QList<Author>& authors)
{
    if (messages.isEmpty() && authors.isEmpty())
    {
        return;
    }

    if (messages.count() != authors.count())
    {
        qWarning() << "The number of messages is not equal to the number of authors";
        return;
    }

    AxelChat::ServiceType serviceType = AxelChat::ServiceType::Unknown;

    QList<std::shared_ptr<Message>> messagesValidToAdd;
    QList<Author*> updatedAuthors;

    for (int i = 0; i < messages.count(); ++i)
    {
        Message&& message = std::move(messages[i]);
        if (messagesModel.contains(message.getId()) && !message.isHasFlag(Message::Flag::DeleterItem))
        {
            continue;
        }

        Author& author = authors[i];

        messagesModel.insertAuthor(author);

        if (!message.isHasFlag(Message::Flag::DeleterItem))
        {
            switch (author.getServiceType())
            {
            case AxelChat::ServiceType::Unknown:
            case AxelChat::ServiceType::Software:
                break;

            default:
                serviceType = author.getServiceType();
                updatedAuthors.append(&author);
                break;
            }
        }

        messagesValidToAdd.append(std::move(std::make_shared<Message>(message)));
    }

    outputToFile.writeAuthors(updatedAuthors);

    if (messagesValidToAdd.isEmpty())
    {
        return;
    }

    outputToFile.writeMessages(messagesValidToAdd, serviceType);
    webSocket.sendMessages(messagesValidToAdd);

    for (int i = 0; i < messagesValidToAdd.count(); ++i)
    {
        const std::shared_ptr<Message>& message = messagesValidToAdd[i];

#ifndef AXELCHAT_LIBRARY
        if (!message->isHasFlag(Message::Flag::IgnoreBotCommand))
        {
            bot.processMessage(message);
        }
#endif

        messagesModel.append(std::move(message));
    }

    emit messagesDataChanged();

    if (_enabledSoundNewMessage && !messages.empty())
    {
        playNewMessageSound();
    }
}

void ChatHandler::sendTestMessage(const QString &text)
{
    const Author& author = Author::getSoftwareAuthor();
    QList<Author> authors;
    authors.append(author);

    Message message({new Message::Text(text)}, author);
    message.setCustomAuthorName(tr("Test Message"));
    message.setCustomAuthorAvatarUrl(QUrl("qrc:/resources/images/flask2.svg"));

    QList<Message> messages;
    messages.append(message);

    onReadyRead(messages, authors);
}

void ChatHandler::sendSoftwareMessage(const QString &text)
{
    const Author& author = Author::getSoftwareAuthor();
    QList<Author> authors;
    authors.append(author);

    QList<Message> messages;
    messages.append(Message({new Message::Text(text)},
                                author,
                                QDateTime::currentDateTime(),
                                QDateTime::currentDateTime(),
                                QString(),
                                {},
                                {}));

    onReadyRead(messages, authors);
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
    qWarning() << Q_FUNC_INFO << ": module multimedia not included";
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

    emit viewersTotalCountChanged();
}

#ifndef AXELCHAT_LIBRARY
void ChatHandler::openProgramFolder()
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(QString("file:///") + QCoreApplication::applicationDirPath()));
}
#endif

void ChatHandler::onConnectedChanged(const bool connected)
{
    ChatService* service = qobject_cast<ChatService*>(sender());
    if (!service)
    {
        qCritical() << Q_FUNC_INFO << "!service";
        return;
    }

    if (!connected && _enabledClearMessagesOnLinkChange)
    {
        clearMessages();
    }

    emit connectedCountChanged();
}

void ChatHandler::onAuthorNameChanged(const Author& author, const QString &prevName, const QString &newName)
{
    if (_enableShowAuthorNameChanged)
    {
        sendSoftwareMessage(tr("%1: \"%2\" changed name to \"%3\"")
                            .arg(ChatService::getName(author.getServiceType()))
                            .arg(prevName, newName));
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

    for (ChatService* chat : qAsConst(services))
    {
        chat->reconnect();
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

    ChatService* service = services.at(index);
    services.removeAt(index);
    delete service;
}

template<typename ChatServiceInheritedClass>
void ChatHandler::addService()
{
    static_assert(std::is_base_of<ChatService, ChatServiceInheritedClass>::value, "ChatServiceInheritedClass must derive from ChatService");

    ChatServiceInheritedClass* service = new ChatServiceInheritedClass(settings, SettingsGroupPath, network, web, this);

    QObject::connect(service, &ChatService::stateChanged, this, &ChatHandler::onStateChanged);
    QObject::connect(service, &ChatService::readyRead, this, &ChatHandler::onReadyRead);
    QObject::connect(service, &ChatService::connectedChanged, this, &ChatHandler::onConnectedChanged);
    QObject::connect(service, &ChatService::authorDataUpdated, this, &ChatHandler::onAuthorDataUpdated);

    services.append(service);
}

void ChatHandler::addTestMessages()
{
    QList<Message> messages;
    QList<Author> authors;

    {
        Author author(AxelChat::ServiceType::YouTube, "Mario", QUuid::createUuid().toString(), QUrl("https://static.wikia.nocookie.net/mario/images/e/e3/MPS_Mario.png/revision/latest/scale-to-width-down/350?cb=20220814154953"));
        authors.append(author);

        QList<Message::Content*> contents;
        contents.append(new Message::Text("Hello it's me Mario!"));

        messages.append(Message(contents, author, QDateTime::currentDateTime(), QDateTime::currentDateTime(), QUuid::createUuid().toString()));
    }

    {
        Author author(AxelChat::ServiceType::Twitch, "BigSmoke", QUuid::createUuid().toString(), QUrl("https://static.wikia.nocookie.net/gtawiki/images/b/bf/BigSmoke-GTASAde.png/revision/latest/scale-to-width-down/350?cb=20211113214309"));
        authors.append(author);

        QList<Message::Content*> contents;
        contents.append(new Message::Text("i’ll have two number 9s, a number 9 large, a number 6 with extra dip, a number 7, two number 45s, one with cheese, and a large soda"));

        messages.append(Message(contents, author, QDateTime::currentDateTime(), QDateTime::currentDateTime(), QUuid::createUuid().toString()));
    }

    {
        Author author(AxelChat::ServiceType::Trovo,
                      "Luigi", QUuid::createUuid().toString(),
                      QUrl("https://static.wikia.nocookie.net/mario/images/7/72/MPSS_Luigi.png/revision/latest/scale-to-width-down/254?cb=20220705200355"),
                      {},
                      {},
                      {},
                      {},
                      QColor("#FF00C7"),
                      QColor("#FFFFFF"));
        authors.append(author);

        QList<Message::Content*> contents;
        contents.append(new Message::Text("Mamma mia!"));

        messages.append(Message(contents,
                                author,
                                QDateTime::currentDateTime(),
                                QDateTime::currentDateTime(),
                                QUuid::createUuid().toString(),
                                {},
                                { { Message::ColorRole::BodyBackgroundColorRole, QColor("#A8D9FF") } },
                                { "Underground" }));
    }

    {
        Author author(AxelChat::ServiceType::GoodGame,
                      "CJ", QUuid::createUuid().toString(),
                      QUrl("https://static.wikia.nocookie.net/gtawiki/images/2/29/CarlJohnson-GTASAde-Infobox.png/revision/latest/scale-to-width-down/350?cb=20211113054252"),
                      {},
                      { "https://static.wikia.nocookie.net/gtawiki/images/2/29/CarlJohnson-GTASAde-Infobox.png/revision/latest/scale-to-width-down/350?cb=20211113054252", "https://static.wikia.nocookie.net/gtawiki/images/2/29/CarlJohnson-GTASAde-Infobox.png/revision/latest/scale-to-width-down/350?cb=20211113054252" },
                      { "https://static.wikia.nocookie.net/gtawiki/images/2/29/CarlJohnson-GTASAde-Infobox.png/revision/latest/scale-to-width-down/350?cb=20211113054252", "https://static.wikia.nocookie.net/gtawiki/images/2/29/CarlJohnson-GTASAde-Infobox.png/revision/latest/scale-to-width-down/350?cb=20211113054252" },
                      {});
        authors.append(author);

        QList<Message::Content*> contents;
        contents.append(new Message::Text("Here We Go Again"));

        messages.append(Message(contents, author, QDateTime::currentDateTime(), QDateTime::currentDateTime(), QUuid::createUuid().toString()));
    }

    {
        Author author(AxelChat::ServiceType::VkPlayLive, "Kenneth Rosenberg", QUuid::createUuid().toString(), QUrl("https://static.wikia.nocookie.net/p__/images/b/b2/Ken_rosenberg.jpg/revision/latest?cb=20130915190559&path-prefix=protagonist"));
        authors.append(author);

        QList<Message::Content*> contents;
        contents.append(new Message::Text("Hey, just like old times, huh, Tommy?"));

        messages.append(Message(contents, author, QDateTime::currentDateTime(), QDateTime::currentDateTime(), QUuid::createUuid().toString()));
    }

    {
        Author author(AxelChat::ServiceType::Telegram, "G-Man", QUuid::createUuid().toString(), QUrl("https://static.wikia.nocookie.net/half-life/images/4/41/G-Man_Alyx_Trailer.jpg/revision/latest/scale-to-width-down/350?cb=20191122020607&path-prefix=en"),
                      {}, {}, {}, {}, {}, {});
        authors.append(author);

        QList<Message::Content*> contents;
        contents.append(new Message::Text("Rise and shine, Mister Freeman"));

        messages.append(Message(contents, author, QDateTime::currentDateTime(), QDateTime::currentDateTime(), QUuid::createUuid().toString(),
                                {}, {}, { "Xen", "Stasis" }));
    }

    {
        Author author(AxelChat::ServiceType::Discord, "Gordon Freeman", QUuid::createUuid().toString(), QUrl("https://static.wikia.nocookie.net/half-life/images/1/1f/GordonALYX.png/revision/latest/scale-to-width-down/350?cb=20220520125500&path-prefix=en"));
        authors.append(author);

        QList<Message::Content*> contents;
        contents.append(new Message::Text("May I say a few words?"));

        messages.append(Message(contents, author, QDateTime::currentDateTime(), QDateTime::currentDateTime(), QUuid::createUuid().toString()));
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

void ChatHandler::setEnabledClearMessagesOnLinkChange(bool enabled)
{
    if (_enabledClearMessagesOnLinkChange != enabled)
    {
        _enabledClearMessagesOnLinkChange = enabled;

        settings.setValue(SettingsEnabledClearMessagesOnLinkChange, enabled);

        emit enabledClearMessagesOnLinkChangeChanged();
    }
}

int ChatHandler::connectedCount() const
{
    int result = 0;

    for (ChatService* service : services)
    {
        if (service->getConnectionStateType()  == ChatService::ConnectionStateType::Connected)
        {
            result++;
        }
    }

    return result;
}

int ChatHandler::getViewersTotalCount() const
{
    int result = 0;

    for (ChatService* service : services)
    {
        const int count = service->getViewersCount();
        if (count >= 0)
        {
            result += count;
        }
    }

    return result;
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

    return services.at(index);
}

ChatService *ChatHandler::getServiceByType(int type) const
{
    for (ChatService* service : services)
    {
        if (service->getServiceType() == (AxelChat::ServiceType)type)
        {
            return service;
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
