#include "chathandler.hpp"
#include "models/chatauthor.h"
#include "models/chatmessage.h"
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

ChatHandler::ChatHandler(QSettings& settings_, QNetworkAccessManager& network_, QObject *parent)
    : QObject(parent)
    , settings(settings_)
    , network(network_)
    , outputToFile(settings, SettingsGroupPath + "/output_to_file", network, messagesModel)
    , bot(settings, SettingsGroupPath + "/chat_bot")
    , authorQMLProvider(*this, messagesModel, outputToFile)
{
    connect(&outputToFile, &OutputToFile::authorNameChanged, this, &ChatHandler::onAuthorNameChanged);

    setEnabledSoundNewMessage(settings.value(SettingsEnabledSoundNewMessage, _enabledSoundNewMessage).toBool());

    setEnabledClearMessagesOnLinkChange(settings.value(SettingsEnabledClearMessagesOnLinkChange, _enabledClearMessagesOnLinkChange).toBool());

    setEnabledShowAuthorNameChanged(settings.value(SettingsEnabledShowAuthorNameChanged, _enableShowAuthorNameChanged).toBool());

    setProxyEnabled(settings.value(SettingsProxyEnabled, _enabledProxy).toBool());
    setProxyServerAddress(settings.value(SettingsProxyAddress, _proxy.hostName()).toString());
    setProxyServerPort(settings.value(SettingsProxyPort, _proxy.port()).toInt());

    youTube = new YouTube(settings, SettingsGroupPath + "/youtube", network, this);
    addService(*youTube);

    twitch = new Twitch(settings, SettingsGroupPath + "/twitch", network, this);
    connect(twitch, &Twitch::avatarDiscovered, this, &ChatHandler::onAvatarDiscovered);
    addService(*twitch);

    goodGame = new GoodGame(settings, SettingsGroupPath + "/goodgame", network, this);
    addService(*goodGame);
}

YouTube &ChatHandler::getYoutube()
{
    return *youTube;
}

Twitch &ChatHandler::getTwitch() const
{
    return *twitch;
}

GoodGame &ChatHandler::getGoodGame() const
{
    return *goodGame;
}

void ChatHandler::onReadyRead(QList<ChatMessage>& messages, QList<ChatAuthor>& authors)
{
    if (messages.count() != authors.count())
    {
        qWarning() << "The number of messages is not equal to the number of authors";
        return;
    }

    QList<ChatMessage> messagesValidToAdd;
    QList<ChatAuthor*> updatedAuthors;

    for (int i = 0; i < messages.count(); ++i)
    {
        ChatMessage&& message = std::move(messages[i]);
        if (messagesModel.contains(message.getId()) && !message.isHasFlag(ChatMessage::Flags::DeleterItem))
        {
            continue;
        }

        ChatAuthor&& author = std::move(authors[i]);

        ChatAuthor* resultAuthor = nullptr;
        ChatAuthor* prevAuthor = messagesModel.getAuthor(author.getId());
        if (prevAuthor)
        {
            const auto prevMessagesCount = prevAuthor->getMessagesCount();
            *prevAuthor = author;
            prevAuthor->setMessagesCount(prevMessagesCount + 1);
            resultAuthor = prevAuthor;
        }
        else
        {
            ChatAuthor* newAuthor = new ChatAuthor();
            *newAuthor = author;
            newAuthor->setMessagesCount(1);
            messagesModel.addAuthor(newAuthor);
            resultAuthor = newAuthor;
        }

        if (resultAuthor && !message.isHasFlag(ChatMessage::Flags::DeleterItem))
        {
            switch (resultAuthor->getServiceType())
            {
            case ChatService::ServiceType::Unknown:
            case ChatService::ServiceType::Software:
                break;

            default:
                updatedAuthors.append(resultAuthor);
                break;
            }
        }

        messagesValidToAdd.append(std::move(message));
    }

    outputToFile.writeAuthors(updatedAuthors);

    if (messagesValidToAdd.isEmpty())
    {
        return;
    }

    outputToFile.writeMessages(messagesValidToAdd);

    for (int i = 0; i < messagesValidToAdd.count(); ++i)
    {
        ChatMessage&& message = std::move(messagesValidToAdd[i]);

#ifndef AXELCHAT_LIBRARY
        if (!message.isHasFlag(ChatMessage::Flags::IgnoreBotCommand))
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
    const ChatAuthor& author = ChatAuthor::getSoftwareAuthor();
    QList<ChatAuthor> authors;
    authors.append(author);

    ChatMessage message({new ChatMessage::Text(text)}, author.getId());
    message.setCustomAuthorName(tr("Test Message"));
    message.setCustomAuthorAvatarUrl(QUrl("qrc:/resources/images/flask2.svg"));

    QList<ChatMessage> messages;
    messages.append(message);

    onReadyRead(messages, authors);
}

void ChatHandler::sendSoftwareMessage(const QString &text)
{
    const ChatAuthor& author = ChatAuthor::getSoftwareAuthor();
    QList<ChatAuthor> authors;
    authors.append(author);

    QList<ChatMessage> messages;
    messages.append(ChatMessage({new ChatMessage::Text(text)},
                                author.getId(),
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

void ChatHandler::onAvatarDiscovered(const QString &channelId, const QUrl &url)
{
    ChatService::ServiceType type = ChatService::ServiceType::Unknown;
    ChatService* service = qobject_cast<ChatService*>(sender());
    if (service)
    {
        type = service->getServiceType();
    }

    outputToFile.downloadAvatar(channelId, url, type);
    messagesModel.applyAvatar(channelId, url);
}

void ChatHandler::clearMessages()
{
    messagesModel.clear();
}

void ChatHandler::onStateChanged()
{
    if (YouTube* youTube = qobject_cast<YouTube*>(sender()); youTube)
    {
        outputToFile.setYouTubeInfo(youTube->getInfo());
    }
    else if (qobject_cast<Twitch*>(sender()); twitch)
    {
        outputToFile.setTwitchInfo(twitch->getInfo());
    }

    emit viewersTotalCountChanged();
}

#ifndef AXELCHAT_LIBRARY
void ChatHandler::openProgramFolder()
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(QString("file:///") + QCoreApplication::applicationDirPath()));
}
#endif

void ChatHandler::onConnected(QString name)
{
    ChatService* service = qobject_cast<ChatService*>(sender());
    if (!service)
    {
        return;
    }

    sendSoftwareMessage(tr("%1 connected: %2").arg(service->getNameLocalized()).arg(name));

    emit connectedCountChanged();
}

void ChatHandler::onDisconnected(QString name)
{
    ChatService* service = qobject_cast<ChatService*>(sender());
    if (!service)
    {
        return;
    }

    sendSoftwareMessage(tr("%1 disconnected: %2").arg(service->getNameLocalized()).arg(name));

    if (_enabledClearMessagesOnLinkChange)
    {
        clearMessages();
    }

    emit connectedCountChanged();
}

void ChatHandler::onAuthorNameChanged(const ChatAuthor& author, const QString &prevName, const QString &newName)
{
    if (_enableShowAuthorNameChanged)
    {
        sendSoftwareMessage(tr("%1: \"%2\" changed name to \"%3\"")
                            .arg(ChatService::getNameLocalized(author.getServiceType()))
                            .arg(prevName)
                            .arg(newName));
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

void ChatHandler::addService(ChatService& service)
{
    services.append(&service);

    connect(&service, &ChatService::readyRead, this, &ChatHandler::onReadyRead);
    connect(&service, &ChatService::connected, this, &ChatHandler::onConnected);
    connect(&service, &ChatService::disconnected, this, &ChatHandler::onDisconnected);
    connect(&service, &ChatService::stateChanged, this, &ChatHandler::onStateChanged);
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

ChatMessagesModel& ChatHandler::getMessagesModel()
{
    return messagesModel;
}

#ifdef QT_QUICK_LIB
void ChatHandler::declareQml()
{
    qmlRegisterUncreatableType<ChatHandler> ("AxelChat.ChatHandler",
                                             1, 0, "ChatHandler", "Type cannot be created in QML");

    qmlRegisterUncreatableType<YouTube> ("AxelChat.YouTube",
                                                    1, 0, "YouTube", "Type cannot be created in QML");

    qmlRegisterUncreatableType<Twitch> ("AxelChat.Twitch",
                                                    1, 0, "Twitch", "Type cannot be created in QML");

    qmlRegisterUncreatableType<OutputToFile> ("AxelChat.OutputToFile",
                                              1, 0, "OutputToFile", "Type cannot be created in QML");

    qmlRegisterUncreatableType<ChatService> ("AxelChat.ChatService",
                                              1, 0, "ChatService", "Type cannot be created in QML");

    AuthorQMLProvider::declareQML();
    ChatBot::declareQml();
}
#endif

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

int ChatHandler::viewersTotalCount() const
{
    int result = 0;

    for (ChatService* service : services)
    {
        if (service->getConnectionStateType()  == ChatService::ConnectionStateType::Connected)
        {
            const int count = service->getViewersCount();
            if (count < 0)
            {
                return -1;
            }
            else
            {
                result += count;
            }
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
    if (index >= services.count())
    {
        return nullptr;
    }

    return services.at(index);
}

ChatService *ChatHandler::getServiceByType(int type) const
{
    for (ChatService* service : services)
    {
        if (service->getServiceType() == (ChatService::ServiceType)type)
        {
            return service;
        }
    }

    return nullptr;
}

QUrl ChatHandler::getServiceIconUrl(int serviceType) const
{
    return ChatService::getIconUrl((ChatService::ServiceType)serviceType);
}

QUrl ChatHandler::getServiceNameLocalized(int serviceType) const
{
    return ChatService::getNameLocalized((ChatService::ServiceType)serviceType);
}
