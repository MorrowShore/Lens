#include "donationalerts.h"
#include "secrets.h"
#include "crypto/obfuscator.h"
namespace
{

static const QString ClientID = OBFUSCATE(DONATIONALERTS_CLIENT_ID);

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
    config.scope = "oauth-donation-subscribe+oauth-donation-index";
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
        const QJsonObject authInfo = auth.getAuthorizationInfo();

        authStateInfo->setItemProperty("text", "<img src=\"qrc:/resources/images/tick.svg\" width=\"20\" height=\"20\"> " + tr("Logged in"));
        loginButton->setItemProperty("text", tr("Logout"));
        reconnect();
        break;
    }
    }

    emit stateChanged();
}

void DonationAlerts::reconnectImpl()
{

}
