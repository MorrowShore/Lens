#include "youtubebrowser.h"
#include "youtubeutils.h"
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>

YouTubeBrowser::YouTubeBrowser(QSettings& settings, const QString& settingsGroupPathParent, QNetworkAccessManager&, cweqt::Manager& web_, QObject *parent)
    : ChatService(settings, settingsGroupPathParent, AxelChat::ServiceType::YouTube, true, parent)
    , web(web_)
{

}

ChatService::ConnectionStateType YouTubeBrowser::getConnectionStateType() const
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

QString YouTubeBrowser::getStateDescription() const
{
    switch (getConnectionStateType())
    {
    case ConnectionStateType::NotConnected:
        if (stream.get().isEmpty())
        {
            return tr("Broadcast not specified");
        }

        if (state.streamId.isEmpty())
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

void YouTubeBrowser::reconnectImpl()
{
    if (state.connected)
    {
        emit connectedChanged(false);
    }

    state = State();

    state.streamId = YouTubeUtils::extractBroadcastId(stream.get().trimmed());

    if (!state.streamId.isEmpty())
    {
        state.chatUrl = QUrl(QString("https://www.youtube.com/live_chat?v=%1").arg(state.streamId));

        state.streamUrl = QUrl(QString("https://www.youtube.com/watch?v=%1").arg(state.streamId));

        state.controlPanelUrl = QUrl(QString("https://studio.youtube.com/video/%1/livestreaming").arg(state.streamId));
    }

    if (!enabled.get())
    {
        return;
    }

    cweqt::Browser::Settings settings;

    settings.visible = false;
    settings.showResponses = true;
    settings.filter.urlPrefixes = { "https://www.youtube.com/youtubei/v1/live_chat/get_live_chat" };

    if (browser)
    {
        browser->close();
        browser.reset();
    }

    if (browser = web.createBrowser(state.chatUrl, settings); browser)
    {
        //connect(browser.get(), &cweqt::Browser::opened, this, []() { qDebug() << Q_FUNC_INFO << "Browser opened"; });

        connect(browser.get(), &cweqt::Browser::closed, this, [this]()
        {
            qDebug() << Q_FUNC_INFO << "Browser closed";
            browser.reset();
        });

        connect(browser.get(), QOverload<std::shared_ptr<cweqt::Response>>::of(&cweqt::Browser::recieved), this, [this](std::shared_ptr<cweqt::Response> response)
        {
            if (!response)
            {
                qWarning() << Q_FUNC_INFO << "Response is null";
                return;
            }

            const QJsonValue v = QJsonDocument::fromJson(response->data).object()
                .value("continuationContents").toObject()
                .value("liveChatContinuation").toObject()
                .value("actions");

            if (v.type() != QJsonValue::Type::Array)
            {
                qWarning() << Q_FUNC_INFO << "array of actions not found";
                return;
            }

            if (!state.connected && !state.streamId.isEmpty() && enabled.get())
            {
                state.connected = true;

                emit connectedChanged(true);
                emit stateChanged();
            }

            const QJsonArray actions = v.toArray();

            emit actionsReceived(actions, response->data);
        });
    }
    else
    {
        qWarning() << Q_FUNC_INFO << "failed to create browser";
    }
}

void YouTubeBrowser::openWindow()
{
    if (!browser)
    {
        qWarning() << Q_FUNC_INFO << "browser is null";
        return;
    }

    if (QWindow* window = browser->getWindow(); window)
    {
        window->setVisible(true);
    }
    else
    {
        qDebug() << Q_FUNC_INFO << "Window is null";
    }
}
