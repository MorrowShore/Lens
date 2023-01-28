#include "telegram.h"
#include <QDesktopServices>
#include <QNetworkReply>

namespace
{

static const int MaxBadChatReplies = 10;
static const int RequestChatInterval = 2000;

}

Telegram::Telegram(QSettings& settings_, const QString& settingsGroupPath, QNetworkAccessManager& network_, QObject *parent)
    : ChatService(settings_, settingsGroupPath, AxelChat::ServiceType::Telegram, parent)
    , settings(settings_)
    , network(network_)
{
    getParameter(stream)->setName(tr("Bot token"));
    getParameter(stream)->setPlaceholder("0000000000:AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA");
    getParameter(stream)->setFlag(Parameter::Flag::PasswordEcho);

    parameters.append(Parameter(new Setting<QString>(settings, QString()), tr("1. Create a bot with @BotFather\n2. Add the bot to the desired group/channel and give it admin rights\n3. Specify your bot token above"), Parameter::Type::Label));

    parameters.append(Parameter(new Setting<QString>(settings, QString()), tr("Create bot with @BotFather"), Parameter::Type::Button, {}, [](const QVariant&)
    {
        QDesktopServices::openUrl(QUrl("https://telegram.me/botfather"));
    }));

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

            const QJsonObject jsonUser = QJsonDocument::fromJson(rawData).object().value("result").toObject();

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
}
