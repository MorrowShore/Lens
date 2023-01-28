#include "telegram.h"
#include "models/message.h"
#include <QDesktopServices>
#include <QNetworkReply>

namespace
{

static const int MaxBadChatReplies = 10;
static const int RequestChatInterval = 3000;
static const int RequestChatTimeoutSeconds = 2;
}

Telegram::Telegram(QSettings& settings_, const QString& settingsGroupPath, QNetworkAccessManager& network_, QObject *parent)
    : ChatService(settings_, settingsGroupPath, AxelChat::ServiceType::Telegram, parent)
    , settings(settings_)
    , network(network_)
    , allowPrivateChat(settings_, "allow_private_chats", false)
{
    getParameter(stream)->setName(tr("Bot token"));
    getParameter(stream)->setPlaceholder("0000000000:AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA");
    getParameter(stream)->setFlag(Parameter::Flag::PasswordEcho);

    parameters.append(Parameter::createLabel(tr("1. Create a bot with @BotFather\n2. Add the bot to the desired groups or channels\n3. Give admin rights to the bot in these groups/channels\n4. Specify your bot token above")));

    parameters.append(Parameter::createButton(tr("Create bot with @BotFather"), [](const QVariant&)
    {
        QDesktopServices::openUrl(QUrl("https://telegram.me/botfather"));
    }));

    parameters.append(Parameter::createSwitch(&allowPrivateChat, tr("Allow private chats (at one's own risk)")));

    QObject::connect(&timerRequestChat, &QTimer::timeout, this, &Telegram::requestChat);
    timerRequestChat.start(RequestChatInterval);

    reconnect();
}

void Telegram::reconnect()
{
    if (state.connected)
    {
        emit connectedChanged(false, info.botUserName);
    }

    state = State();
    info = Info();

    state.streamId = stream.get().trimmed();

    emit stateChanged();

    requestChat();
}

ChatService::ConnectionStateType Telegram::getConnectionStateType() const
{
    if (state.connected)
    {
        return ChatService::ConnectionStateType::Connected;
    }
    else if (!state.streamId.isEmpty())
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
        if (stream.get().isEmpty())
        {
            return tr("Bot token not specified");
        }

        if (state.streamId.isEmpty())
        {
            return tr("Bot token is not correct");
        }

        return tr("Not connected");

    case ConnectionStateType::Connecting:
        return tr("Connecting...");

    case ConnectionStateType::Connected:
        return tr("Successfully connected!");

    }

    return "<unknown_state>";
}

void Telegram::processBadChatReply()
{
    info.badChatReplies++;

    if (info.badChatReplies >= MaxBadChatReplies)
    {
        if (state.connected && !state.streamId.isEmpty())
        {
            qWarning() << Q_FUNC_INFO << "too many bad chat replies! Disonnecting...";

            state = State();

            state.connected = false;

            emit connectedChanged(false, info.botUserName);

            reconnect();
        }
    }
}

void Telegram::requestChat()
{
    if (state.streamId.isEmpty())
    {
        return;
    }

    if (info.botUserName.isEmpty())
    {
        QNetworkRequest request(QUrl(QString("https://api.telegram.org/bot%1/getMe").arg(state.streamId)));
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
            const QString botUserName = jsonUser.value("username").toString();

            if (botUserId != 0 && !botUserName.isEmpty())
            {
                info.badChatReplies = 0;
                info.botUserId = botUserId;
                info.botUserName = botUserName;

                if (!state.connected)
                {
                    state.connected = true;
                    emit connectedChanged(true, info.botUserName);
                    emit stateChanged();
                }
            }
        });
    }
    else
    {
        QNetworkRequest request(
                    QUrl(
                        QString("https://api.telegram.org/bot%1/getUpdates?timeout=%2&allowed_updates=message")
                        .arg(state.streamId)
                        .arg(RequestChatTimeoutSeconds)
                        ));
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

            if (!state.connected)
            {
                state.connected = true;
                emit connectedChanged(true, info.botUserName);
                emit stateChanged();
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
            else if (key == "update_id" ||
                     key == "my_chat_member" // TODO
                     )
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

    const QDateTime dateTime = QDateTime::fromMSecsSinceEpoch(jsonMessage.value("date").toVariant().toLongLong());

    const QString messageId = chatId + "/" + QString("%1").arg(jsonMessage.value("message_id").toVariant().toLongLong());

    if (!jsonMessage.contains("text"))
    {
        return;
    }

    const QString messageText = jsonMessage.value("text").toString();

    const QList<Message::Content*> contents = { new Message::Text(messageText) };

    const Message message(contents, author, dateTime, QDateTime::currentDateTime(), messageId);

    messages.append(message);
    authors.append(author);

    if (!usersPhotoUpdated.contains(userId))
    {
        requestUserPhoto(author.getId(), userId);
    }
}

void Telegram::requestUserPhoto(const QString& authorId, const int64_t& userId)
{
    QNetworkRequest request(
                QUrl(
                    QString("https://api.telegram.org/bot%1/getUserProfilePhotos?user_id=%2&limit=1")
                    .arg(state.streamId)
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
            qWarning() << Q_FUNC_INFO << "photos is empty";
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
    QNetworkRequest request(
                QUrl(
                    QString("https://api.telegram.org/bot%1/getFile?file_id=%2")
                    .arg(state.streamId)
                    .arg(fileId)
                    ));

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

        const QUrl avatarUrl = QUrl(QString("https://api.telegram.org/file/bot%1/%2").arg(state.streamId).arg(filePath));

        emit authorDataUpdated(authorId, { {Author::Role::AvatarUrl, avatarUrl} });
    });
}
