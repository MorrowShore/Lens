#include "vkvideo.h"
#include "secrets.h"
#include "crypto/obfuscator.h"
#include <QNetworkRequest>
#include <QNetworkReply>

namespace
{

static const int RequestChatInterval = 2000;

static const QString ClientId = OBFUSCATE(VK_APP_ID);
static const QString ApiVersion = "5.131";

static bool checkReply(QNetworkReply *reply, const char *tag, QByteArray& resultData)
{
    resultData.clear();

    if (!reply)
    {
        qWarning() << tag << ": !reply";
        return false;
    }

    resultData = reply->readAll();
    reply->deleteLater();
    if (resultData.isEmpty())
    {
        qWarning() << tag << ": data is empty";
        return false;
    }

    const QJsonObject root = QJsonDocument::fromJson(resultData).object();
    if (root.contains("error"))
    {
        qWarning() << tag << "Error:" << resultData << ", request =" << reply->request().url().toString();
        return false;
    }

    return true;
}

}

VkVideo::VkVideo(QSettings &settings, const QString &settingsGroupPath, QNetworkAccessManager &network_, QObject *parent)
    : ChatService(settings, settingsGroupPath, AxelChat::ServiceType::VkVideo, parent)
    , network(network_)
    , authStateInfo(UIElementBridge::createLabel("Loading..."))
    , auth(settings, settingsGroupPath + "/auth", network)
{
    getUIElementBridgeBySetting(stream)->setItemProperty("placeholderText", tr("Broadcast link..."));

    addUIElement(authStateInfo);

    OAuth2::Config config;
    config.flowType = OAuth2::FlowType::AuthorizationCode;
    config.clientId = ClientId;
    config.clientSecret = OBFUSCATE(VK_SECURE_KEY);
    config.authorizationPageUrl = QString("https://oauth.vk.com/authorize?v=%1&display=page").arg(ApiVersion);
    config.redirectUrl = "http://localhost:" + QString("%1").arg(TcpServer::Port) + "/chat_service/" + getServiceTypeId(getServiceType()) + "/auth_code";
    config.scope = "video+offline";
    config.requestTokenUrl = "https://oauth.vk.com/access_token";
    auth.setConfig(config);
    QObject::connect(&auth, &OAuth2::stateChanged, this, &VkVideo::updateUI);

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

    QObject::connect(&timerRequestChat, &QTimer::timeout, this, &VkVideo::onTimeoutRequestChat);
    timerRequestChat.start(RequestChatInterval);

    updateUI();
    reconnect();
}

ChatService::ConnectionStateType VkVideo::getConnectionStateType() const
{
    if (state.connected)
    {
        return ChatService::ConnectionStateType::Connected;
    }
    else if (enabled.get() && !state.streamId.isEmpty())
    {
        return ChatService::ConnectionStateType::Connecting;
    }

    return ChatService::ConnectionStateType::NotConnected;
}

QString VkVideo::getStateDescription() const
{
    switch (getConnectionStateType())
    {
    case ConnectionStateType::NotConnected:
        if (stream.get().isEmpty())
        {
            return tr("Broadcast not specified");
        }

        if (info.ownerId.isEmpty() || info.videoId.isEmpty())
        {
            return tr("The broadcast is not correct");
        }

        return tr("Not connected");

    case ConnectionStateType::Connecting:
        return tr("Connecting...");

    case ConnectionStateType::Connected:
        return tr("Successfully connected!");

    }

    return "<unknown_state>";
}

TcpReply VkVideo::processTcpRequest(const TcpRequest &request)
{
    const QString path = request.getUrl().path().toLower();

    if (path == "/auth_code")
    {
        return auth.processRedirect(request);
    }

    return TcpReply::createTextHtmlError("Unknown path");
}

void VkVideo::reconnectImpl()
{
    state = State();
    info = Info();

    if (!extractOwnerVideoId(stream.get().trimmed(), info.ownerId, info.videoId))
    {
        emit stateChanged();
        return;
    }

    state.streamUrl = QUrl(QString("https://vk.com/video/lives?z=video%1_%2").arg(info.ownerId, info.videoId));

    onTimeoutRequestChat();
    updateUI();
}

void VkVideo::onTimeoutRequestChat()
{
    if (!enabled.get() || !auth.isLoggedIn() || info.ownerId.isEmpty() || info.videoId.isEmpty())
    {
        return;
    }

    QNetworkRequest request(QString("https://api.vk.com/method/video.getComments?access_token=%1&v=%2&start_comment_id=0&count=30&sort=desc&owner_id=%3&video_id=%4")
                            .arg(auth.getAccessToken(), ApiVersion, info.ownerId, info.videoId));
    QNetworkReply* reply = network.get(request);
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]()
    {
        QByteArray data;
        if (!checkReply(reply, Q_FUNC_INFO, data))
        {
            return;
        }

        const QJsonObject root = QJsonDocument::fromJson(data).object().value("users").toObject();
        qDebug() << root;
    });
}

void VkVideo::updateUI()
{
    if (!authStateInfo)
    {
        qCritical() << Q_FUNC_INFO << "!authStateInfo";
    }

    switch (auth.getState())
    {
    case OAuth2::State::NotLoggedIn:
        authStateInfo->setItemProperty("text", "<img src=\"qrc:/resources/images/error-alt-svgrepo-com.svg\" width=\"20\" height=\"20\"> " + tr("You are not logged in"));
        loginButton->setItemProperty("text", tr("Login"));
        break;

    case OAuth2::State::LoginInProgress:
        authStateInfo->setItemProperty("text", tr("Login in progress..."));
        loginButton->setItemProperty("text", tr("Login"));
        break;

    case OAuth2::State::LoggedIn:
        authStateInfo->setItemProperty("text", "<img src=\"qrc:/resources/images/tick.svg\" width=\"20\" height=\"20\"> " + tr("Logged in"));
        loginButton->setItemProperty("text", tr("Logout"));
        break;
    }

    emit stateChanged();
}

bool VkVideo::extractOwnerVideoId(const QString &videoiLink, QString &ownerId, QString &videoId)
{
    //TODO

    ownerId = "454393491";
    videoId = "456239750";

    return true;
}
