#include "telegram.h"
#include "models/message.h"
#include <QDesktopServices>
#include <QNetworkReply>

namespace
{

static const int MaxBadChatReplies = 10;
static const int RequestChatInterval = 4000;
static const int RequestChatTimeoutSeconds = 3;
}

Telegram::Telegram(QSettings& settings_, const QString& settingsGroupPath, QNetworkAccessManager& network_, QObject *parent)
    : ChatService(settings_, settingsGroupPath, AxelChat::ServiceType::Telegram, parent)
    , settings(settings_)
    , network(network_)
    , botToken(settings_, settingsGroupPath + "/bot_token")
    , showChatTitle(settings_, "show_chat_title", true)
    , allowPrivateChat(settings_, "allow_private_chats", false)
{
    getParameter(stream)->resetFlag(Parameter::Flag::Visible);

    parameters.append(Parameter::createLineEdit(&botToken, tr("Bot token"), "0000000000:AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA", { Parameter::Flag::PasswordEcho }));

    parameters.append(Parameter::createLabel(tr("1. Create a bot with @BotFather\n"
                                                "2. Add the bot to the desired groups or channels\n"
                                                "3. Give admin rights to the bot in these groups/channels\n"
                                                "4. Specify your bot token above\n"
                                                "\n"
                                                "It is not recommended to use a bot that is already being used for other purposes\n"
                                                "It is not recommended to use more than one %1 with the same bot").arg(QCoreApplication::applicationName())));

    parameters.append(Parameter::createButton(tr("Create bot with @BotFather"), [](const QVariant&)
    {
        QDesktopServices::openUrl(QUrl("https://telegram.me/botfather"));
    }));

    parameters.append(Parameter::createSwitch(&showChatTitle, tr("Show chat name when possible")));
    parameters.append(Parameter::createSwitch(&allowPrivateChat, tr("Allow private chats (at one's own risk)")));

    QObject::connect(&timerRequestUpdates, &QTimer::timeout, this, &Telegram::requestUpdates);
    timerRequestUpdates.start(RequestChatInterval);

    reconnect();
}

void Telegram::reconnectImpl()
{
    if (state.connected)
    {
        emit connectedChanged(false, QString());
    }

    state = State();
    info = Info();

    if (!enabled.get())
    {
        return;
    }

    requestUpdates();
}

ChatService::ConnectionStateType Telegram::getConnectionStateType() const
{
    if (state.connected)
    {
        return ChatService::ConnectionStateType::Connected;
    }
    else if (enabled.get() && !botToken.get().isEmpty())
    {
        return ChatService::ConnectionStateType::Connecting;
    }

    return ChatService::ConnectionStateType::NotConnected;
}

QString Telegram::getStateDescription() const
{
    switch (getConnectionStateType())
    {
    case ConnectionStateType::NotConnected:
        if (botToken.get().isEmpty())
        {
            return tr("Bot token not specified");
        }

        return tr("Not connected");

    case ConnectionStateType::Connecting:
        return tr("Connecting...");

    case ConnectionStateType::Connected:
        return tr("Successfully connected!");

    }

    return "<unknown_state>";
}

void Telegram::onParameterChangedImpl(Parameter &parameter)
{
    Setting<QString>* setting = parameter.getSettingString();
    if (!setting)
    {
        return;
    }

    if (*&setting == &botToken)
    {
        const QString token = setting->get().trimmed();
        setting->set(token);
        reconnect();
    }
}

void Telegram::processBadChatReply()
{
    info.badChatReplies++;

    if (info.badChatReplies >= MaxBadChatReplies)
    {
        if (state.connected && !botToken.get().isEmpty())
        {
            qWarning() << Q_FUNC_INFO << "too many bad chat replies! Disonnecting...";

            state = State();

            state.connected = false;

            emit connectedChanged(false, QString());

            reconnect();
        }
    }
}

void Telegram::requestUpdates()
{
    if (!enabled.get() || botToken.get().isEmpty())
    {
        return;
    }

    if (info.botUserId == -1)
    {
        QNetworkRequest request(QUrl(QString("https://api.telegram.org/bot%1/getMe").arg(botToken.get())));
        QNetworkReply* reply = network.get(request);
        if (!reply)
        {
            qDebug() << Q_FUNC_INFO << ": !reply";
            return;
        }

        QObject::connect(reply, &QNetworkReply::finished, this, [this]()
        {
            QNetworkReply *reply = dynamic_cast<QNetworkReply*>(sender());
            if (!reply)
            {
                qDebug() << Q_FUNC_INFO << "!reply";
                return;
            }

            const QByteArray rawData = reply->readAll();
            reply->deleteLater();

            if (!enabled.get())
            {
                return;
            }

            if (rawData.isEmpty())
            {
                processBadChatReply();
                qDebug() << Q_FUNC_INFO << ":rawData is empty";
                return;
            }

            const QJsonObject root = QJsonDocument::fromJson(rawData).object();
            if (!root.value("ok").toBool())
            {
                qDebug() << Q_FUNC_INFO << "error:" << root;;
                return;
            }

            const QJsonObject jsonUser = root.value("result").toObject();

            const int64_t botUserId = jsonUser.value("id").toVariant().toLongLong(0);

            if (botUserId != 0)
            {
                info.badChatReplies = 0;
                info.botUserId = botUserId;

                if (!state.connected || !botToken.get().trimmed().isEmpty())
                {
                    state.connected = true;
                    emit connectedChanged(true, QString());
                    emit stateChanged();
                }
            }
        });
    }
    else
    {
        QString url = QString("https://api.telegram.org/bot%1/getUpdates?timeout=%2&allowed_updates=message")
                .arg(botToken.get())
                .arg(RequestChatTimeoutSeconds);

        if (info.lastUpdateId != -1)
        {
            url += QString("&offset=%1").arg(info.lastUpdateId + 1);
        }

        QNetworkReply* reply = network.get(QNetworkRequest(QUrl(url)));
        if (!reply)
        {
            qDebug() << Q_FUNC_INFO << ": !reply";
            return;
        }

        QObject::connect(reply, &QNetworkReply::finished, this, [this]()
        {
            QNetworkReply *reply = dynamic_cast<QNetworkReply*>(sender());
            if (!reply)
            {
                qDebug() << Q_FUNC_INFO << "!reply";
                return;
            }

            const QByteArray rawData = reply->readAll();
            reply->deleteLater();

            if (!enabled.get())
            {
                return;
            }

            if (rawData.isEmpty())
            {
                processBadChatReply();
                qDebug() << Q_FUNC_INFO << ":rawData is empty";
                return;
            }

            const QJsonObject root = QJsonDocument::fromJson(rawData).object();
            if (!root.value("ok").toBool())
            {
                qDebug() << Q_FUNC_INFO << "error:" << root;;
                return;
            }

            parseUpdates(root.value("result").toArray());
        });
    }
}

void Telegram::parseUpdates(const QJsonArray& updates)
{
    QList<Message> messages;
    QList<Author> authors;

    for (const QJsonValue& v : updates)
    {
        const QJsonObject jsonUpdate = v.toObject();

        //qDebug() << "\n" << jsonUpdate << "\n";

        const QStringList keys = jsonUpdate.keys();
        for (const QString& key : qAsConst(keys))
        {
            if (key == "message")
            {
                parseMessage(jsonUpdate.value(key).toObject(), messages, authors);
            }
            else if (key == "update_id")
            {
                info.lastUpdateId = jsonUpdate.value("update_id").toVariant().toLongLong();
            }
            else if (key == "my_chat_member") // TODO
            {
                // ignore
            }
            else
            {
                qWarning() << Q_FUNC_INFO << "unknown key" << key;
            }
        }
    }

    if (!authors.isEmpty() && !messages.isEmpty())
    {
        emit readyRead(messages, authors);
    }
}

void Telegram::parseMessage(const QJsonObject &jsonMessage, QList<Message> &messages, QList<Author> &authors)
{
    const QJsonObject jsonChat = jsonMessage.value("chat").toObject();

    const QString chatId = QString("%1").arg(jsonChat.value("id").toVariant().toLongLong());
    const QString chatType = jsonChat.value("type").toString();

    if (chatType == "private" && !allowPrivateChat.get())
    {
        return;
    }

    const QJsonObject jsonFrom = jsonMessage.value("from").toObject();

    const int64_t userId = jsonFrom.value("id").toVariant().toLongLong();

    QString authorName = jsonFrom.value("first_name").toString();

    const QString lastName = jsonFrom.value("last_name").toString();
    if (!lastName.isEmpty())
    {
        authorName += " " + lastName;
    }

    const Author author(getServiceType(), authorName, QString("%1").arg(userId));

    const QDateTime dateTime = QDateTime::fromSecsSinceEpoch(jsonMessage.value("date").toVariant().toLongLong());

    const QString messageId = chatId + "/" + QString("%1").arg(jsonMessage.value("message_id").toVariant().toLongLong());

    QList<Message::Content*> contents;

    if (jsonMessage.contains("caption"))
    {
        if (!contents.isEmpty()) { contents.append(new Message::Text("\n")); }

        Message::Text::Style style;
        style.bold = true;

        const QString text = jsonMessage.value("caption").toString();
        contents.append(new Message::Text(text, style));
    }

    if (jsonMessage.contains("text"))
    {
        if (!contents.isEmpty()) { contents.append(new Message::Text("\n")); }

        const QString messageText = jsonMessage.value("text").toString();
        contents.append(new Message::Text(messageText));
    }

    if (jsonMessage.contains("animation")) { addServiceContent(contents, tr("animation")); }
    if (jsonMessage.contains("audio")) { addServiceContent(contents, tr("audio")); }
    if (jsonMessage.contains("document")) { addServiceContent(contents, tr("document")); }
    if (jsonMessage.contains("photo")) { addServiceContent(contents, tr("image")); }
    if (jsonMessage.contains("sticker")) { addServiceContent(contents, tr("sticker")); }
    if (jsonMessage.contains("video")) { addServiceContent(contents, tr("video")); }
    if (jsonMessage.contains("video_note")) { addServiceContent(contents, tr("video note")); }
    if (jsonMessage.contains("voice")) { addServiceContent(contents, tr("voice note")); }
    if (jsonMessage.contains("contact")) { addServiceContent(contents, tr("contact")); }
    if (jsonMessage.contains("dice")) { addServiceContent(contents, tr("dice")); }
    if (jsonMessage.contains("game")) { addServiceContent(contents, tr("game")); }
    if (jsonMessage.contains("poll")) { addServiceContent(contents, tr("poll")); }
    if (jsonMessage.contains("venue")) { addServiceContent(contents, tr("venue")); }
    if (jsonMessage.contains("location")) { addServiceContent(contents, tr("location")); }
    if (jsonMessage.contains("video_chat_scheduled")) { addServiceContent(contents, tr("video chat scheduled")); }
    if (jsonMessage.contains("video_chat_started")) { addServiceContent(contents, tr("video chat started")); }
    if (jsonMessage.contains("video_chat_ended")) { addServiceContent(contents, tr("video chat ended")); }

    if (!contents.isEmpty())
    {
        QString destination;

        if (showChatTitle.get())
        {
            destination = jsonChat.value("title").toString().trimmed();
        }

        const Message message(contents, author, dateTime, QDateTime::currentDateTime(), messageId, {}, {}, destination);

        messages.append(message);
        authors.append(author);

        if (!usersPhotoUpdated.contains(userId))
        {
            requestUserPhoto(author.getId(), userId);
        }
    }
}

void Telegram::requestUserPhoto(const QString& authorId, const int64_t& userId)
{
    QNetworkRequest request(
                QUrl(
                    QString("https://api.telegram.org/bot%1/getUserProfilePhotos?user_id=%2&limit=1")
                    .arg(botToken.get())
                    .arg(userId)
                    ));

    QNetworkReply* reply = network.get(request);
    if (!reply)
    {
        qDebug() << Q_FUNC_INFO << ": !reply";
        return;
    }

    QObject::connect(reply, &QNetworkReply::finished, this, [this, authorId, userId]()
    {
        QNetworkReply *reply = dynamic_cast<QNetworkReply*>(sender());
        if (!reply)
        {
            qDebug() << Q_FUNC_INFO << "!reply";
            return;
        }

        const QByteArray rawData = reply->readAll();
        reply->deleteLater();

        if (rawData.isEmpty())
        {
            processBadChatReply();
            qDebug() << Q_FUNC_INFO << ":rawData is empty";
            return;
        }

        const QJsonObject root = QJsonDocument::fromJson(rawData).object();
        if (!root.value("ok").toBool())
        {
            qDebug() << Q_FUNC_INFO << "error:" << root;;
            return;
        }

        usersPhotoUpdated.insert(userId);

        const QJsonArray jsonPhotos = root.value("result").toObject().value("photos").toArray();
        if (jsonPhotos.isEmpty())
        {
            //qWarning() << Q_FUNC_INFO << "photos is empty";
            return;
        }

        const QJsonArray jsonPhotosSizes = jsonPhotos.at(0).toArray();
        if (jsonPhotosSizes.isEmpty())
        {
            qWarning() << Q_FUNC_INFO << "photos sizes is empty";
            return;
        }

        static const int PhotoSupportedMaxSize = 640;
        int maxSize = 0;
        int maxSizeIndex = 0;

        for (int i = 0; i < jsonPhotosSizes.count(); i++)
        {
            const QJsonValue v = jsonPhotosSizes[i];
            const QJsonObject jsonPhoto = v.toObject();
            const int size = jsonPhoto.value("height").toInt();

            if (size > maxSize && size <= PhotoSupportedMaxSize)
            {
                maxSize = size;
                maxSizeIndex = i;
            }
        }

        const QString fileId = jsonPhotosSizes[maxSizeIndex].toObject().value("file_id").toString();
        requestPhotoFileInfo(authorId, fileId);
    });
}

void Telegram::requestPhotoFileInfo(const QString& authorId, const QString &fileId)
{
    const QNetworkRequest request(QUrl(QString("https://api.telegram.org/bot%1/getFile?file_id=%2").arg(botToken.get(), fileId)));

    QNetworkReply* reply = network.get(request);
    if (!reply)
    {
        qDebug() << Q_FUNC_INFO << ": !reply";
        return;
    }

    QObject::connect(reply, &QNetworkReply::finished, this, [this, authorId]()
    {
        QNetworkReply *reply = dynamic_cast<QNetworkReply*>(sender());
        if (!reply)
        {
            qDebug() << Q_FUNC_INFO << "!reply";
            return;
        }

        const QByteArray rawData = reply->readAll();
        reply->deleteLater();

        if (rawData.isEmpty())
        {
            processBadChatReply();
            qDebug() << Q_FUNC_INFO << ":rawData is empty";
            return;
        }

        const QJsonObject root = QJsonDocument::fromJson(rawData).object();
        if (!root.value("ok").toBool())
        {
            qDebug() << Q_FUNC_INFO << "error:" << root;;
            return;
        }

        const QString filePath = root.value("result").toObject().value("file_path").toString();
        if (filePath.isEmpty())
        {
            qDebug() << Q_FUNC_INFO << "file path os empty";
            return;
        }

        const QUrl avatarUrl = QUrl(QString("https://api.telegram.org/file/bot%1/%2").arg(botToken.get(), filePath));

        emit authorDataUpdated(authorId, { {Author::Role::AvatarUrl, avatarUrl} });
    });
}

void Telegram::addServiceContent(QList<Message::Content *>& contents, const QString &name_)
{
    if (!contents.isEmpty()) { contents.append(new Message::Text("\n")); }

    QString name = name_;

    if (!name.isEmpty())
    {
        name.replace(0, 1, name.at(0).toUpper());
    }

    Message::Text::Style style;
    style.italic = true;
    contents.append(new Message::Text("[" + name + "]", style));
}
