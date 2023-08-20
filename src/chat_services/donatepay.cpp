#include "donatepay.h"
#include <QDesktopServices>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

namespace
{

static const int ReconncectPeriod = 6 * 1000;
static const int PingSendTimeout = 10 * 1000;
static const int CheckPingSendTimeout = PingSendTimeout * 1.5;

}

DonatePay::DonatePay(QSettings& settings, const QString& settingsGroupPathParent, QNetworkAccessManager& network_, cweqt::Manager&, QObject *parent)
    : ChatService(settings, settingsGroupPathParent, AxelChat::ServiceType::DonatePayRu, false, parent)
    , network(network_)
    , apiKey(settings, getSettingsGroupPath() + "/api_key", QString(), true)
    , domain("https://donatepay.ru")
    , donationPagePrefix("https://new.donatepay.ru/@")
{
    getUIElementBridgeBySetting(stream)->setItemProperty("visible", false);

    authStateInfo = std::shared_ptr<UIElementBridge>(UIElementBridge::createLabel("Loading..."));
    addUIElement(authStateInfo);

    addUIElement(std::shared_ptr<UIElementBridge>(UIElementBridge::createLineEdit(&apiKey, tr("API key"), "AbCdEfGhIjKlMnOpQrStUvWxYz0123456789", true)));

    addUIElement(std::shared_ptr<UIElementBridge>(UIElementBridge::createButton(tr("Get API key"), [this]()
    {
        QDesktopServices::openUrl(QUrl(domain + "/page/api"));
    })));

    donatePageButton = std::shared_ptr<UIElementBridge>(UIElementBridge::createButton(tr("Donation page"), [this]()
    {
        QDesktopServices::openUrl(QUrl(donationPagePrefix + info.userId));
    }));
    addUIElement(donatePageButton);

    addUIElement(std::shared_ptr<UIElementBridge>(UIElementBridge::createButton(tr("Transactions"), [this]()
    {
        QDesktopServices::openUrl(QUrl(domain + "/billing/transactions"));
    })));

    QObject::connect(&socket, &QWebSocket::stateChanged, this, [](QAbstractSocket::SocketState state){
        Q_UNUSED(state)
        //qDebug() << Q_FUNC_INFO << "webSocket state changed:" << state;
    });

    QObject::connect(&socket, &QWebSocket::textMessageReceived, this, &DonatePay::onReceiveWebSocket);

    QObject::connect(&socket, &QWebSocket::connected, this, [this]()
    {
        //qDebug() << Q_FUNC_INFO << "webSocket connected";

        checkPingTimer.setInterval(CheckPingSendTimeout);
        checkPingTimer.start();

        send({
            { "token", info.socketToken },
        });
    });

    QObject::connect(&socket, &QWebSocket::disconnected, this, [this]()
    {
        //qDebug() << Q_FUNC_INFO << "webSocket disconnected";

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

    /*QObject::connect(&timerReconnect, &QTimer::timeout, this, [this]()
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
    timerReconnect.start(ReconncectPeriod);*/

    QObject::connect(&pingTimer, &QTimer::timeout, this, &DonatePay::sendPing);
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
    reconnect();
}

ChatService::ConnectionStateType DonatePay::getConnectionStateType() const
{
    if (state.connected)
    {
        return ChatService::ConnectionStateType::Connected;
    }
    else if (enabled.get())
    {
        return ChatService::ConnectionStateType::Connecting;
    }

    return ChatService::ConnectionStateType::NotConnected;
}

QString DonatePay::getStateDescription() const
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

void DonatePay::updateUI()
{
    if (info.userId.isEmpty())
    {
        authStateInfo->setItemProperty("text", "<img src=\"qrc:/resources/images/error-alt-svgrepo-com.svg\" width=\"20\" height=\"20\"> " + tr("Not authorized"));
        donatePageButton->setItemProperty("enabled", false);
    }
    else
    {
        authStateInfo->setItemProperty("text", "<img src=\"qrc:/resources/images/tick.svg\" width=\"20\" height=\"20\"> " + tr("Authorized as %1").arg("<b>" + info.userName + "</b>"));
        donatePageButton->setItemProperty("enabled", true);
    }
}

void DonatePay::requestUser()
{
    const QString key = apiKey.get();
    if (key.isEmpty())
    {
        qWarning() << Q_FUNC_INFO << "API key is empty";
        return;
    }

    QNetworkReply* reply = network.get(QNetworkRequest(domain + "/api/v1/user?access_token=" + key));
    connect(reply, &QNetworkReply::finished, this, [this, reply]()
    {
        const QByteArray rawData = reply->readAll();
        const QJsonObject root = QJsonDocument::fromJson(rawData).object();
        const QString status = root.value("status").toString();
        if (status != "success")
        {
            const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

            if (statusCode == 200)
            {
                qWarning() << Q_FUNC_INFO << "status" << status << ", root =" << root;
            }
            else
            {
                qWarning() << Q_FUNC_INFO << "status" << status << ", status code =" << statusCode;
            }

            return;
        }

        const QJsonObject data = root.value("data").toObject();

        info.userId = QString("%1").arg(data.value("id").toVariant().toLongLong());
        info.userName = data.value("name").toString();

        updateUI();
    });
}

void DonatePay::requestSocketToken()
{
    const QString key = apiKey.get();
    if (key.isEmpty())
    {
        qWarning() << Q_FUNC_INFO << "API key is empty";
        return;
    }

    QNetworkRequest request(domain + "/api/v2/socket/token");

    request.setRawHeader("Content-Type", "application/json");

    const QByteArray data = QJsonDocument(QJsonObject({ { "access_token", key } })).toJson();

    QNetworkReply* reply = network.post(request, data);
    connect(reply, &QNetworkReply::finished, this, [this, reply]()
    {
        const QByteArray rawData = reply->readAll();
        const QJsonObject root = QJsonDocument::fromJson(rawData).object();

        info.socketToken = root.value("token").toString();

        if (!info.socketToken.isEmpty())
        {
            socket.open(QUrl("wss://centrifugo.donatepay.ru:43002/connection/websocket"));
        }
        else
        {
            qDebug() << "token is empty, root =" << root;
        }
    });
}

void DonatePay::requestSubscribeCentrifuge(const QString &clientId, const QString &userId)
{
    QNetworkRequest request(QString(domain + "/api/v2/centrifuge/subscribe"));
    //request.setRawHeader("Authorization", QByteArray("Bearer ") + auth.getAccessToken().toUtf8());
    request.setRawHeader("Content-Type", "application/json");

    QJsonObject data;

    data.insert("client", clientId);

    data.insert("channels", QJsonArray({"$public:" + userId}));

    QNetworkReply* reply = network.post(request, QJsonDocument(data).toJson(QJsonDocument::JsonFormat::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply]()
    {
        qDebug() << reply->readAll();

        //sendConnectToChannels(channelsInfo);
    });
}

void DonatePay::onReceiveWebSocket(const QString &rawData)
{
    if (!enabled.get())
    {
        return;
    }

    checkPingTimer.setInterval(CheckPingSendTimeout);
    checkPingTimer.start();

    const QJsonDocument doc = QJsonDocument::fromJson(rawData.toUtf8());

    qDebug() << "received:" << doc;

    const QJsonObject root = doc.object();

    const QString client = root.value("result").toObject().value("client").toString();
    if (!client.isEmpty())
    {
        requestSubscribeCentrifuge(client, info.userId);
        return;
    }
}

void DonatePay::reconnectImpl()
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

    timerReconnect.start();
    requestUser();
    requestSocketToken();
}

void DonatePay::onUiElementChangedImpl(const std::shared_ptr<UIElementBridge> element)
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

    if (*&setting == &apiKey)
    {
        const QString apiKey = setting->get().trimmed();
        setting->set(apiKey);
        reconnect();
    }
}

void DonatePay::send(const QJsonObject &params, const int method)
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

    qDebug() << "send:" << doc;

    socket.sendTextMessage(doc.toJson(QJsonDocument::JsonFormat::Compact));
}

void DonatePay::sendConnectToChannels(const QString& socketToken, const QString& client)
{
    send({
        { "token", socketToken },
        { "channel", client },
    }, 1);
}

void DonatePay::sendPing()
{
    if (socket.state() != QAbstractSocket::SocketState::ConnectedState)
    {
        return;
    }

    static const int MethodHeartbeat = 7;

    send(QJsonObject(), MethodHeartbeat);
}