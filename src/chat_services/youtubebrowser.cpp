#include "youtubebrowser.h"
#include "youtubeutils.h"
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>

namespace
{

static const int ReconncectPeriod = 5 * 1000;
static const int CheckConnectionTimeout = 20 * 1000;
static const int RequestStreamInterval = 10000;

}

YouTubeBrowser::YouTubeBrowser(QSettings& settings, const QString& settingsGroupPathParent, QNetworkAccessManager& network_, cweqt::Manager& web_, QObject *parent)
    : ChatService(settings, settingsGroupPathParent, AxelChat::ServiceType::YouTube, true, parent)
    , network(network_)
    , web(web_)
{
    uiBridge.findBySetting(stream)->setItemProperty("placeholderText", tr("Link or broadcast ID..."));

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

    QObject::connect(&web, &cweqt::Manager::initialized, this, [this]()
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

    QObject::connect(&timerCheckConnection, &QTimer::timeout, this, [this]()
    {
        if (!enabled.get())
        {
            return;
        }

        if (state.connected)
        {
            reconnect();
        }
    });
    timerCheckConnection.setInterval(CheckConnectionTimeout);

    QObject::connect(&timerRequestStreamPage, &QTimer::timeout, this, &YouTubeBrowser::requestStreamPage);
    timerRequestStreamPage.start(RequestStreamInterval);
    
    openChatButton = std::shared_ptr<UIBridgeElement>(UIBridgeElement::createButton(tr("Open chat"), [this]()
    {
        openWindow();
    }));
    openChatButton->setItemProperty("enabled", false);
    uiBridge.addElement(openChatButton);
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
    const bool preConnected = state.connected;

    state = State();

    timerCheckConnection.stop();

    if (preConnected)
    {
        emit connectedChanged(false);
    }

    openChatButton->setItemProperty("enabled", false);

    state.streamId = YouTubeUtils::extractBroadcastId(stream.get().trimmed());

    if (!state.streamId.isEmpty())
    {
        state.chatUrl = QUrl(QString("https://www.youtube.com/live_chat?v=%1").arg(state.streamId));

        state.streamUrl = QUrl(QString("https://www.youtube.com/watch?v=%1").arg(state.streamId));

        state.controlPanelUrl = QUrl(QString("https://studio.youtube.com/video/%1/livestreaming").arg(state.streamId));
    }

    if (!enabled.get() || !web.isInitialized())
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
            reconnect();
        });

        connect(browser.get(), QOverload<std::shared_ptr<cweqt::Response>>::of(&cweqt::Browser::recieved), this, [this](std::shared_ptr<cweqt::Response> response)
        {
            if (!response)
            {
                qWarning() << Q_FUNC_INFO << "Response is null";
                return;
            }

            const QJsonObject root = QJsonDocument::fromJson(response->data).object();

            const QJsonValue v = root
                .value("continuationContents").toObject()
                .value("liveChatContinuation").toObject();

            if (v.type() != QJsonValue::Type::Object)
            {
                qWarning() << Q_FUNC_INFO << "liveChatContinuation not found, object =" << root;
                return;
            }

            timerCheckConnection.start();

            if (!state.connected && !state.streamId.isEmpty() && enabled.get())
            {
                state.connected = true;

                openChatButton->setItemProperty("enabled", true);

                requestStreamPage();

                emit connectedChanged(true);
                emit stateChanged();
            }

            const QJsonArray actions = v.toObject().value("actions").toArray();

            QList<std::shared_ptr<Message>> messages;
            QList<std::shared_ptr<Author>> authors;

            YouTubeUtils::parseActionsArray(actions, response->data, messages, authors);

            if (!messages.isEmpty())
            {
                emit readyRead(messages, authors);
            }
        });
    }
    else
    {
        qWarning() << Q_FUNC_INFO << "failed to create browser";
    }
}

void YouTubeBrowser::requestStreamPage()
{
    if (!enabled.get() || state.streamUrl.isEmpty())
    {
        return;
    }

    QNetworkRequest request(state.streamUrl);
    request.setHeader(QNetworkRequest::KnownHeaders::UserAgentHeader, AxelChat::UserAgentNetworkHeaderName);
    request.setRawHeader("Accept-Language", YouTubeUtils::AcceptLanguageNetworkHeaderName);

    QNetworkReply* reply = network.get(request);
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]()
    {
        const QByteArray data = reply->readAll();
        reply->deleteLater();

        if (data.isEmpty())
        {
            qDebug() << Q_FUNC_INFO << "data is empty";
            return;
        }

        if (const int viewers = YouTubeUtils::parseViews(data); viewers != -1)
        {
            state.viewersCount = viewers;
            emit stateChanged();
        }
    });
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
