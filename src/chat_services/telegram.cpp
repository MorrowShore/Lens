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

Telegram::Telegram(QSettings& settings, const QString& settingsGroupPathParent, QNetworkAccessManager& network_, cweqt::Manager&, QObject *parent)
    : ChatService(settings, settingsGroupPathParent, AxelChat::ServiceType::Telegram, false, parent)
    , network(network_)
    , botToken(settings, getSettingsGroupPath() + "/bot_token", QString(), true)
    , showChatTitle(settings, getSettingsGroupPath() + "/show_chat_title", true)
    , allowPrivateChat(settings, getSettingsGroupPath() + "/allow_private_chats", false)
{
    ui.findBySetting(stream)->setItemProperty("visible", false);
    
    authStateInfo = ui.addLabel("Loading...");

    ui.addLineEdit(&botToken, tr("Bot token"), "0000000000:AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA", true);
    ui.addLabel("1. " + tr("Create a bot with @BotFather"));
    ui.addButton(tr("Open @BotFather"), []()
    {
        QDesktopServices::openUrl(QUrl("https://telegram.me/botfather"));
    });
    ui.addLabel("2. " + tr("Copy and paste bot token above") + ". <b><font color=\"red\">" + tr("DON'T DISCLOSE THE BOT'S TOKEN!") + "</b></font>");
    ui.addLabel("3. " + tr("Add the bot to the desired groups/channels"));
    ui.addLabel("4. " + tr("Give admin rights to the bot in these groups/channels"));
    ui.addLabel(" ");
    ui.addLabel(tr("It is not recommended to use a bot that is already being used for other purposes"));
    ui.addLabel(tr("It is not recommended to use more than one %1 with the same bot").arg(QCoreApplication::applicationName()));
    ui.addElement(std::shared_ptr<UIBridgeElement>(UIBridgeElement::createSwitch(&showChatTitle, tr("Show chat name when possible"))));
    ui.addElement(std::shared_ptr<UIBridgeElement>(UIBridgeElement::createSwitch(&allowPrivateChat, tr("Allow private chats (at one's own risk)"))));
    
    connect(&ui, QOverload<const std::shared_ptr<UIBridgeElement>&>::of(&UIBridge::elementChanged), this, [this](const std::shared_ptr<UIBridgeElement>& element)
    {
        if (!element)
        {
            qCritical() << Q_FUNC_INFO << "!element";
            return;
        }

        Setting<QString>* setting = element->getSettingString();
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
    });

    QObject::connect(&timerRequestUpdates, &QTimer::timeout, this, &Telegram::requestUpdates);
    timerRequestUpdates.start(RequestChatInterval);

    reconnect();

    updateUI();
}

void Telegram::reconnectImpl()
{
    const bool preConnected = state.connected;

    state = State();
    info = Info();

    updateUI();

    if (preConnected)
    {
        emit connectedChanged(false);
        emit stateChanged();
    }

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

            emit connectedChanged(false);
            emit stateChanged();

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

            const QString userName = jsonUser.value("username").toString();
            const int64_t botUserId = jsonUser.value("id").toVariant().toLongLong(0);

            if (botUserId != 0)
            {
                info.badChatReplies = 0;
                info.botUserId = botUserId;
                info.botDisplayName = userName;

                if (!state.connected || !botToken.get().trimmed().isEmpty())
                {
                    state.connected = true;
                    emit connectedChanged(true);
                    emit stateChanged();
                }

                updateUI();
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
    QList<std::shared_ptr<Message>> messages;
    QList<std::shared_ptr<Author>> authors;

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

void Telegram::parseMessage(const QJsonObject &jsonMessage, QList<std::shared_ptr<Message>> &messages, QList<std::shared_ptr<Author>> &authors)
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

    std::shared_ptr<Author> author = std::make_shared<Author>(
        getServiceType(),
        authorName,
        QString("%1").arg(userId));

    const QDateTime dateTime = QDateTime::fromSecsSinceEpoch(jsonMessage.value("date").toVariant().toLongLong());

    const QString messageId = chatId + "/" + QString("%1").arg(jsonMessage.value("message_id").toVariant().toLongLong());

    QList<std::shared_ptr<Message::Content>> contents;

    if (jsonMessage.contains("caption"))
    {
        if (!contents.isEmpty()) { contents.append(std::make_shared<Message::Text>("\n")); }

        Message::TextStyle style;
        style.bold = true;

        const QString text = jsonMessage.value("caption").toString();
        contents.append(std::make_shared<Message::Text>(text, style));
    }

    if (jsonMessage.contains("text"))
    {
        if (!contents.isEmpty()) { contents.append(std::make_shared<Message::Text>("\n")); }

        const QString messageText = jsonMessage.value("text").toString();
        contents.append(std::make_shared<Message::Text>(messageText));
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
        QStringList destination;

        if (showChatTitle.get())
        {
            const QString title = jsonChat.value("title").toString().trimmed();
            if (!title.isEmpty())
            {
                destination.append(title);
            }
        }

        std::shared_ptr<Message> message = std::make_shared<Message>(
            contents,
            author,
            dateTime,
            QDateTime::currentDateTime(),
            messageId,
            std::set<Message::Flag>(),
            QHash<Message::ColorRole, QColor>(),
            destination);

        messages.append(message);
        authors.append(author);

        if (!usersPhotoUpdated.contains(userId))
        {
            requestUserPhoto(author->getId(), userId);
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

void Telegram::addServiceContent(QList<std::shared_ptr<Message::Content>>& contents, const QString &name_)
{
    if (!contents.isEmpty()) { contents.append(std::make_shared<Message::Text>("\n")); }

    QString name = name_;

    if (!name.isEmpty())
    {
        name.replace(0, 1, name.at(0).toUpper());
    }

    Message::TextStyle style;
    style.italic = true;
    contents.append(std::make_shared<Message::Text>("[" + name + "]", style));
}

void Telegram::updateUI()
{
    QString botStatus = tr("Bot status") + ": ";

    if (state.connected)
    {
        botStatus += tr("authorized as %1").arg("<b>" + info.botDisplayName + "</b>");

        authStateInfo->setItemProperty("text", "<img src=\"qrc:/resources/images/tick.svg\" width=\"20\" height=\"20\"> " + botStatus);
    }
    else
    {
        botStatus += tr("not authorized");
        authStateInfo->setItemProperty("text", "<img src=\"qrc:/resources/images/error-alt-svgrepo-com.svg\" width=\"20\" height=\"20\"> " + botStatus);
    }
}
