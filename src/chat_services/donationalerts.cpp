#include "donationalerts.h"
#include "secrets.h"
#include "crypto/obfuscator.h"
#include "models/message.h"
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QTimeZone>
#include <QDesktopServices>

namespace
{

static const QString ClientID = OBFUSCATE(DONATIONALERTS_CLIENT_ID);
static const int ReconncectPeriod = 6 * 1000;
static const int PingSendTimeout = 10 * 1000;
static const int CheckPingSendTimeout = PingSendTimeout * 1.5;
static const QColor BackgroundColor = QColor(255, 209, 147);
static const QColor AuthorColor = QColor(245, 144, 7);

bool checkReply(QNetworkReply *reply, const char *tag, QByteArray &resultData)
{
    resultData.clear();

    if (!reply)
    {
        qWarning() << tag << tag << ": !reply";
        return false;
    }

    int statusCode = 200;
    const QVariant rawStatusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
    resultData = reply->readAll();
    const QUrl requestUrl = reply->request().url();
    reply->deleteLater();

    if (rawStatusCode.isValid())
    {
        statusCode = rawStatusCode.toInt();

        if (statusCode != 200)
        {
            qWarning() << tag << ": status code:" << statusCode;
        }
    }

    if (resultData.isEmpty() && statusCode != 200)
    {
        qWarning() << tag << ": data is empty";
        return false;
    }

    return true;
}

}

DonationAlerts::DonationAlerts(QSettings &settings, const QString &settingsGroupPathParent, QNetworkAccessManager &network_, cweqt::Manager&, QObject *parent)
    : ChatService(settings, settingsGroupPathParent, AxelChat::ServiceType::DonationAlerts, false, parent)
    , network(network_)
    , auth(settings, getSettingsGroupPath() + "/auth", network)
    , authStateInfo(UIElementBridge::createLabel("Loading..."))
{
    getUIElementBridgeBySetting(stream)->setItemProperty("visible", false);

    addUIElement(authStateInfo);

    OAuth2::Config config;
    config.flowType = OAuth2::FlowType::AuthorizationCode;
    config.clientId = ClientID;
    config.clientSecret = OBFUSCATE(DONATIONALERTS_API_KEY);
    config.authorizationPageUrl = "https://www.donationalerts.com/oauth/authorize";
    config.redirectUrl = "http://localhost:" + QString("%1").arg(TcpServer::Port) + "/chat_service/" + getServiceTypeId(getServiceType()) + "/auth_code";
    config.scope = "oauth-user-show+oauth-donation-subscribe+oauth-donation-index";
    config.requestTokenUrl = "https://www.donationalerts.com/oauth/token";
    config.refreshTokenUrl = "https://www.donationalerts.com/oauth/token";
    auth.setConfig(config);
    QObject::connect(&auth, &OAuth2::stateChanged, this, [this]()
    {
        updateUI();
        reconnect();
    });

    loginButton = std::shared_ptr<UIElementBridge>(UIElementBridge::createButton(tr("Login"), [this]()
    {
        if (auth.isLoggedIn())
        {
            auth.logout();
        }
        else
        {
            auth.login();
        }
    }));
    addUIElement(loginButton);

    addUIElement(std::shared_ptr<UIElementBridge>(UIElementBridge::createButton(tr("Dashboard"), []()
    {
        QDesktopServices::openUrl(QUrl("https://www.donationalerts.com/dashboard"));
    })));

    donateMainPageButton = std::shared_ptr<UIElementBridge>(UIElementBridge::createButton(tr("Donation page"), [this]()
    {
        QDesktopServices::openUrl(QUrl("https://www.donationalerts.com/r/" + info.userName));
    }));
    addUIElement(donateMainPageButton);

    donateAlternativePageButton = std::shared_ptr<UIElementBridge>(UIElementBridge::createButton(tr("Alternative donation page"), [this]()
    {
        QDesktopServices::openUrl(QUrl("https://www.donationalerts.com/c/" + info.userName));
    }));
    addUIElement(donateAlternativePageButton);

    QObject::connect(&socket, &QWebSocket::stateChanged, this, [](QAbstractSocket::SocketState state){
        Q_UNUSED(state)
        //qDebug() << Q_FUNC_INFO << "webSocket state changed:" << state;
    });

    QObject::connect(&socket, &QWebSocket::textMessageReceived, this, &DonationAlerts::onReceiveWebSocket);

    QObject::connect(&socket, &QWebSocket::connected, this, [this]()
    {
        //qDebug() << Q_FUNC_INFO << "webSocket connected";

        checkPingTimer.setInterval(CheckPingSendTimeout);
        checkPingTimer.start();

        send(
        {
            { "token", info.socketConnectionToken },
        });
    });

    QObject::connect(&socket, &QWebSocket::disconnected, this, [this]()
    {
        qDebug() << Q_FUNC_INFO << "webSocket disconnected";

        if (state.connected)
        {
            state.connected = false;
            emit connectedChanged(false);
            emit stateChanged();
        }
    });

    QObject::connect(&socket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error), this, [this](QAbstractSocket::SocketError error_){
        qDebug() << Q_FUNC_INFO << "webSocket error:" << error_ << ":" << socket.errorString();
    });

    QObject::connect(&timerReconnect, &QTimer::timeout, this, [this]()
    {
        if (!enabled.get())
        {
            return;
        }

        if (!state.connected)
        {
            reconnect();
        }
    });
    timerReconnect.start(ReconncectPeriod);

    QObject::connect(&pingTimer, &QTimer::timeout, this, &DonationAlerts::sendPing);
    pingTimer.setInterval(PingSendTimeout);
    pingTimer.start();

    QObject::connect(&checkPingTimer, &QTimer::timeout, this, [this]()
    {
        if (socket.state() != QAbstractSocket::SocketState::ConnectedState)
        {
            checkPingTimer.stop();
            return;
        }

        qDebug() << Q_FUNC_INFO << "check ping timeout, disconnect";
        socket.close();
    });

    updateUI();
}

ChatService::ConnectionStateType DonationAlerts::getConnectionStateType() const
{
    if (state.connected)
    {
        return ChatService::ConnectionStateType::Connected;
    }
    else if (enabled.get() && auth.isLoggedIn())
    {
        return ChatService::ConnectionStateType::Connecting;
    }

    return ChatService::ConnectionStateType::NotConnected;
}

QString DonationAlerts::getStateDescription() const
{
    switch (getConnectionStateType())
    {
    case ConnectionStateType::NotConnected:
        return tr("Not connected");

    case ConnectionStateType::Connecting:
        return tr("Connecting...");

    case ConnectionStateType::Connected:
        return tr("Successfully connected!");
    }

    return "<unknown_state>";
}

TcpReply DonationAlerts::processTcpRequest(const TcpRequest &request)
{
    const QString path = request.getUrl().path().toLower();

    if (path == "/auth_code")
    {
        return auth.processRedirect(request);
    }

    return TcpReply::createTextHtmlError("Unknown path");
}

void DonationAlerts::updateUI()
{
    if (!authStateInfo)
    {
        qCritical() << Q_FUNC_INFO << "!authStateInfo";
    }

    switch (auth.getState())
    {
    case OAuth2::State::NotLoggedIn:
        authStateInfo->setItemProperty("text", "<img src=\"qrc:/resources/images/error-alt-svgrepo-com.svg\" width=\"20\" height=\"20\"> " + tr("Not logged in"));
        loginButton->setItemProperty("text", tr("Login"));
        break;

    case OAuth2::State::LoginInProgress:
        authStateInfo->setItemProperty("text", tr("Login in progress..."));
        loginButton->setItemProperty("text", tr("Login"));
        break;

    case OAuth2::State::LoggedIn:
    {
        authStateInfo->setItemProperty("text", "<img src=\"qrc:/resources/images/tick.svg\" width=\"20\" height=\"20\"> " + tr("Logged in"));
        loginButton->setItemProperty("text", tr("Logout"));
        break;
    }
    }

    if (info.userId.isEmpty())
    {
        donateMainPageButton->setItemProperty("enabled", false);
        donateAlternativePageButton->setItemProperty("enabled", false);
    }
    else
    {
        donateMainPageButton->setItemProperty("enabled", true);
        donateAlternativePageButton->setItemProperty("enabled", true);
    }
}

void DonationAlerts::requestDonations()
{
    QNetworkRequest request(QString("https://www.donationalerts.com/api/v1/alerts/donations"));
    request.setRawHeader("Authorization", QByteArray("Bearer ") + auth.getAccessToken().toUtf8());
    QNetworkReply* reply = network.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]()
     {
        QByteArray data;
        if (!checkReply(reply, Q_FUNC_INFO, data))
        {
            return;
        }

        const QJsonObject root = QJsonDocument::fromJson(data).object();

        //qDebug() << root;
    });
}

void DonationAlerts::requestUser()
{
    QNetworkRequest request(QString("https://www.donationalerts.com/api/v1/user/oauth"));
    request.setRawHeader("Authorization", QByteArray("Bearer ") + auth.getAccessToken().toUtf8());
    QNetworkReply* reply = network.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]()
    {
        QByteArray data;
        if (!checkReply(reply, Q_FUNC_INFO, data))
        {
            return;
        }

        const QJsonObject root = QJsonDocument::fromJson(data).object();
        const QJsonObject dataJson = root.value("data").toObject();

        info.userName = dataJson.value("name").toString();
        info.socketConnectionToken = dataJson.value("socket_connection_token").toString();
        info.userId = QString("%1").arg(dataJson.value("id").toVariant().toLongLong());

        if (info.userName.isEmpty() || info.socketConnectionToken.isEmpty())
        {
            qWarning() << Q_FUNC_INFO << "name or socket token is empty, data =" << root;
            return;
        }

        if (auth.getState() == OAuth2::State::LoggedIn)
        {
            if (authStateInfo)
            {
                authStateInfo->setItemProperty("text", "<img src=\"qrc:/resources/images/tick.svg\" width=\"20\" height=\"20\"> " + tr("Logged in as %1").arg("<b>" + info.userName + "</b>"));
            }

            socket.setProxy(network.proxy());
            socket.open(QUrl("wss://centrifugo.donationalerts.com/connection/websocket"));
        }

        updateUI();
    });
}

void DonationAlerts::requestSubscribeCentrifuge(const QString &clientId, const QString& userId)
{
    QNetworkRequest request(QString("https://www.donationalerts.com/api/v1/centrifuge/subscribe"));
    request.setRawHeader("Authorization", QByteArray("Bearer ") + auth.getAccessToken().toUtf8());
    request.setRawHeader("Content-Type", "application/json");

    QJsonObject data;

    data.insert("client", clientId);

    data.insert("channels", QJsonArray({"$alerts:donation_" + userId}));

    QNetworkReply* reply = network.post(request, QJsonDocument(data).toJson(QJsonDocument::JsonFormat::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply]()
    {
        QByteArray data;
        if (!checkReply(reply, Q_FUNC_INFO, data))
        {
            return;
        }

        QList<PrivateChannelInfo> channelsInfo;

        const QJsonObject root = QJsonDocument::fromJson(data).object();
        const QJsonArray channelsJson = root.value("channels").toArray();
        for (const QJsonValue& v : qAsConst(channelsJson))
        {
            const QJsonObject channelInfoJson = v.toObject();

            PrivateChannelInfo channelInfo;

            channelInfo.channel = channelInfoJson.value("channel").toString();
            channelInfo.token = channelInfoJson.value("token").toString();

            channelsInfo.append(channelInfo);
        }

        sendConnectToChannels(channelsInfo);
    });
}

void DonationAlerts::onReceiveWebSocket(const QString &rawData)
{
    if (!enabled.get())
    {
        return;
    }

    checkPingTimer.setInterval(CheckPingSendTimeout);
    checkPingTimer.start();

    const QJsonDocument doc = QJsonDocument::fromJson(rawData.toUtf8());
    const QJsonObject root = doc.object();

    //qDebug() << "received:" << doc;

    const QJsonObject result = root.value("result").toObject();
    if (result.contains("client"))
    {
        requestSubscribeCentrifuge(result.value("client").toString(), info.userId);
        return;
    }

    if (result.value("type").toInt() == 1 && result.value("channel").toString().startsWith("$"))
    {
        if (!state.connected)
        {
            state.connected = true;
            emit connectedChanged(true);
            emit stateChanged();

            sendPing();
        }

        return;
    }

    if (result.value("channel").toString().startsWith("$"))
    {
        const QJsonValue data = result.value("data").toObject().value("data");
        if (data.isObject())
        {
            parseEvent(data.toObject());
            return;
        }
    }

    if (result.keys() == QStringList{ "epoch", "offset", "recoverable", "seq" })
    {
        return;
    }

    if (root.keys() == QStringList{ "id" })
    {
        // ping
        return;
    }

    qWarning() << Q_FUNC_INFO << "unknown message:" << rawData;
}

void DonationAlerts::send(const QJsonObject &params, const int method)
{
    info.lastMessageId++;

    QJsonObject object;
    object.insert("params", params);
    object.insert("id", info.lastMessageId);

    if (method != -1)
    {
        object.insert("method", method);
    }

    const QJsonDocument doc(object);

    //qDebug() << "send:" << doc;

    socket.sendTextMessage(doc.toJson(QJsonDocument::JsonFormat::Compact));
}

void DonationAlerts::sendConnectToChannels(const QList<PrivateChannelInfo> &channels)
{
    if (channels.isEmpty())
    {
        qWarning() << Q_FUNC_INFO << "channels is empty";
        return;
    }

    for (const PrivateChannelInfo& channel : qAsConst(channels))
    {
        send(
            {
                { "token", channel.token },
                { "channel", channel.channel },
            }, 1);
    }
}

void DonationAlerts::sendPing()
{
    if (socket.state() != QAbstractSocket::SocketState::ConnectedState)
    {
        return;
    }

    static const int MethodHeartbeat = 7;

    send(QJsonObject(), MethodHeartbeat);
}

void DonationAlerts::parseEvent(const QJsonObject &data)
{
    const QString id = data.value("id").toString();
    const QString messageText = data.value("message").toString();
    const double amount = data.value("amount").toDouble();
    const double amountInUserCurrency = data.value("amount_in_user_currency").toDouble(); // TODO: show
    const QString currency = data.value("currency").toString();

    QDateTime sendedTime = QDateTime::fromString(data.value("created_at").toString(), Qt::DateFormat::ISODate);
    sendedTime.setTimeZone(QTimeZone::utc());
    sendedTime = sendedTime.toLocalTime();

    const QJsonValue userNameJson = data.value("username");
    const QString userName = userNameJson.type() == QJsonValue::Type::Null ? tr("Anonymous") : data.value("username").toString();
    //const QString paidSystemTitle = data.value("payin_system").toObject().value("title").toString(); // TODO:

    QList<std::shared_ptr<Message>> messages;
    QList<std::shared_ptr<Author>> authors;

    std::shared_ptr<Author> author = std::make_shared<Author>(
        getServiceType(),
        userName,
        getServiceTypeId(getServiceType()) + "_" + userName,
        QUrl("qrc:/resources/images/money1-icon.svg"),
        QUrl(),
        QStringList(),
        QStringList(),
        std::set<Author::Flag>(),
        AuthorColor);

    QList<std::shared_ptr<Message::Content>> contents;

    {
        Message::TextStyle style;

        style.bold = true;

        const QString header = QString("%1 %2").arg(amount, 0, 'f', 2).arg(currency);

        contents.append(std::make_shared<Message::Text>(header, style));
    }

    const QString messageType = data.value("message_type").toString();
    if (messageType == "text")
    {
        //
    }
    else if (messageType == "audio")
    {
        if (!contents.isEmpty())
        {
            contents.append(std::make_shared<Message::Text>("\n"));
        }

        Message::TextStyle style;

        style.italic = true;

        contents.append(std::make_shared<Message::Text>("[" + tr("Audio") + "]", style));
    }
    else
    {
        qWarning() << Q_FUNC_INFO << "unknown message type" << messageType;
    }

    if (!contents.isEmpty())
    {
        contents.append(std::make_shared<Message::Text>("\n"));

        contents.append(std::make_shared<Message::Text>(messageText));
    }

    std::set<Message::Flag> flags = { Message::Flag::DonateWithText };
    QHash<Message::ColorRole, QColor> forcedColors = { { Message::ColorRole::BodyBackgroundColorRole, BackgroundColor } };

    std::shared_ptr<Message> message = std::make_shared<Message>(
        contents,
        author,
        sendedTime,
        QDateTime::currentDateTime(),
        generateMessageId(id),
        flags,
        forcedColors);

    messages.append(message);
    authors.append(author);

    emit readyRead(messages, authors);
}

void DonationAlerts::reconnectImpl()
{
    socket.close();
    info = Info();

    updateUI();

    if (auth.getState() != OAuth2::State::LoggedIn || !enabled.get())
    {
        return;
    }

    timerReconnect.start();
    requestUser();
    //requestDonations();
}
