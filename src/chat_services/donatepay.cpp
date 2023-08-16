#include "donatepay.h"
#include <QDesktopServices>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

DonatePay::DonatePay(QSettings& settings, const QString& settingsGroupPathParent, QNetworkAccessManager& network_, cweqt::Manager&, QObject *parent)
    : ChatService(settings, settingsGroupPathParent, AxelChat::ServiceType::DonatePayRu, false, parent)
    , network(network_)
    , apiKey(settings, getSettingsGroupPath() + "/api_key", QString(), true)
    , domain("https://donatepay.ru")
{
    getUIElementBridgeBySetting(stream)->setItemProperty("visible", false);

    authStateInfo = std::shared_ptr<UIElementBridge>(UIElementBridge::createLabel("Loading..."));
    addUIElement(authStateInfo);

    addUIElement(std::shared_ptr<UIElementBridge>(UIElementBridge::createLineEdit(&apiKey, tr("API key"), "AbCdEfGhIjKlMnOpQrStUvWxYz0123456789", true)));

    addUIElement(std::shared_ptr<UIElementBridge>(UIElementBridge::createButton(tr("Get API key"), [this]()
    {
        QDesktopServices::openUrl(QUrl(domain + "/page/api"));
    })));

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
    }
    else
    {
        authStateInfo->setItemProperty("text", "<img src=\"qrc:/resources/images/tick.svg\" width=\"20\" height=\"20\"> " + tr("Authorized as %1").arg("<b>" + info.userName + "</b>"));
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

    info = Info();

    QNetworkReply* reply = network.get(QNetworkRequest(domain + "/api/v1/user?access_token=" + key));
    connect(reply, &QNetworkReply::finished, this, [this, reply]()
    {
        const QByteArray rawData = reply->readAll();
        const QJsonObject root = QJsonDocument::fromJson(rawData).object();
        const QString status = root.value("status").toString();
        if (status != "success")
        {
            qWarning() << Q_FUNC_INFO << "status" << status << ", status code =" << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            return;
        }

        const QJsonObject data = root.value("data").toObject();

        info.userId = QString("%1").arg(data.value("id").toVariant().toLongLong());
        info.userName = data.value("name").toString();

        updateUI();
    });
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

    requestUser();
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
