#include "donationalerts.h"
#include "secrets.h"
#include "crypto/obfuscator.h"
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

namespace
{

static const QString ClientID = OBFUSCATE(DONATIONALERTS_CLIENT_ID);

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
    QObject::connect(&auth, &OAuth2::stateChanged, this, &DonationAlerts::onAuthStateChanged);

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

    onAuthStateChanged();
}

ChatService::ConnectionStateType DonationAlerts::getConnectionStateType() const
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

void DonationAlerts::onAuthStateChanged()
{
    if (!authStateInfo)
    {
        qCritical() << Q_FUNC_INFO << "!authStateInfo";
    }

    switch (auth.getState())
    {
    case OAuth2::State::NotLoggedIn:
        authStateInfo->setItemProperty("text", "<img src=\"qrc:/resources/images/error-alt-svgrepo-com.svg\" width=\"20\" height=\"20\"> " + tr("Login for full functionality"));
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
        requestUser();
        reconnect();
        break;
    }
    }

    emit stateChanged();
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

        //qDebug() << data;
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

        const QString name = root.value("data").toObject().value("name").toString();

        if (name.isEmpty())
        {
            qWarning() << Q_FUNC_INFO << "name is empty, data =" << root;
            return;
        }

        if (auth.getState() == OAuth2::State::LoggedIn && authStateInfo)
        {
            authStateInfo->setItemProperty("text", "<img src=\"qrc:/resources/images/tick.svg\" width=\"20\" height=\"20\"> " + tr("Logged in as %1").arg("<b>" + name + "</b>"));
        }
    });
}

void DonationAlerts::reconnectImpl()
{
    requestDonations();
}
