#include "chathandler.h"
#include "chat_services/youtube.h"
#include "chat_services/twitch.h"
#include "chat_services/trovo.h"
#include "chat_services/goodgame.h"
#include "chat_services/vkplaylive.h"
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

ChatHandler::ChatHandler(QSettings& settings_, QNetworkAccessManager& network_, QObject *parent)
    : QObject(parent)
    , settings(settings_)
    , network(network_)
    , outputToFile(settings, SettingsGroupPath + "/output_to_file", network, messagesModel, services)
    , bot(settings, SettingsGroupPath + "/chat_bot")
    , authorQMLProvider(*this, messagesModel, outputToFile)
    , webSocket(*this)
{
    connect(&outputToFile, &OutputToFile::authorNameChanged, this, &ChatHandler::onAuthorNameChanged);

    setEnabledSoundNewMessage(settings.value(SettingsEnabledSoundNewMessage, _enabledSoundNewMessage).toBool());

    setEnabledClearMessagesOnLinkChange(settings.value(SettingsEnabledClearMessagesOnLinkChange, _enabledClearMessagesOnLinkChange).toBool());

    setEnabledShowAuthorNameChanged(settings.value(SettingsEnabledShowAuthorNameChanged, _enableShowAuthorNameChanged).toBool());

    setProxyEnabled(settings.value(SettingsProxyEnabled, _enabledProxy).toBool());
    setProxyServerAddress(settings.value(SettingsProxyAddress, _proxy.hostName()).toString());
    setProxyServerPort(settings.value(SettingsProxyPort, _proxy.port()).toInt());

    addService(new YouTube      (settings, SettingsGroupPath + "/youtube",      network, this));
    addService(new Twitch       (settings, SettingsGroupPath + "/twitch",       network, this));
    addService(new Trovo        (settings, SettingsGroupPath + "/trovo",        network, this));
    addService(new GoodGame     (settings, SettingsGroupPath + "/goodgame",     network, this));
    addService(new VkPlayLive   (settings, SettingsGroupPath + "/vkplaylive",   network, this));
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

    QList<Message> messagesValidToAdd;
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

        messagesValidToAdd.append(std::move(message));
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
        Message&& message = std::move(messagesValidToAdd[i]);

#ifndef AXELCHAT_LIBRARY
        if (!message.isHasFlag(Message::Flag::IgnoreBotCommand))
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
    AxelChat::ServiceType type = AxelChat::ServiceType::Unknown;
    ChatService* service = qobject_cast<ChatService*>(sender());
    if (service)
    {
        type = service->getServiceType();
    }


    const QList<Author::Role> roles = values.keys();
    for (const Author::Role role : roles)
    {
        if (role == Author::Role::AvatarUrl)
        {
            outputToFile.downloadAvatar(authorId, type, values[role].toUrl());
        }
    }

    messagesModel.setAuthorValues(authorId, values);
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
    webSocket.sendInfo();

    emit viewersTotalCountChanged();
}

#ifndef AXELCHAT_LIBRARY
void ChatHandler::openProgramFolder()
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(QString("file:///") + QCoreApplication::applicationDirPath()));
}
#endif

void ChatHandler::onConnectedChanged(const bool connected, const QString& name)
{
    ChatService* service = qobject_cast<ChatService*>(sender());
    if (!service)
    {
        return;
    }

    if (connected)
    {
        sendSoftwareMessage(tr("%1 connected: %2").arg(service->getName()).arg(name));
    }
    else
    {
        sendSoftwareMessage(tr("%1 disconnected: %2").arg(service->getName()).arg(name));

        if (_enabledClearMessagesOnLinkChange)
        {
            clearMessages();
        }
    }

    emit connectedCountChanged();
}

void ChatHandler::onAuthorNameChanged(const Author& author, const QString &prevName, const QString &newName)
{
    if (_enableShowAuthorNameChanged)
    {
        sendSoftwareMessage(tr("%1: \"%2\" changed name to \"%3\"")
                            .arg(ChatService::getName(author.getServiceType()))
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

void ChatHandler::addService(ChatService* service)
{
    connect(service, &ChatService::stateChanged, this, &ChatHandler::onStateChanged);
    connect(service, &ChatService::readyRead, this, &ChatHandler::onReadyRead);
    connect(service, &ChatService::connectedChanged, this, &ChatHandler::onConnectedChanged);
    connect(service, &ChatService::authorDataUpdated, this, &ChatHandler::onAuthorDataUpdated);

    services.append(service);
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
