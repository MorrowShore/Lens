#include "youtubebrowser.h"
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>

YouTubeBrowser::YouTubeBrowser(cweqt::Manager& web_, QObject *parent)
    : QObject(parent)
    , web(web_)
{

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

void YouTubeBrowser::reconnect(const QUrl& chatUrl)
{
    cweqt::Browser::Settings settings;

    settings.visible = false;
    settings.showResponses = true;
    settings.filter.urlPrefixes = { "https://www.youtube.com/youtubei/v1/live_chat/get_live_chat" };

    if (browser)
    {
        browser->close();
        browser.reset();
    }

    if (browser = web.createBrowser(chatUrl, settings); browser)
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

            const QJsonArray actions = QJsonDocument::fromJson(response->data).object()
                .value("continuationContents").toObject()
                .value("liveChatContinuation").toObject()
                .value("actions").toArray();

            emit actionsReceived(actions, response->data);
        });
    }
    else
    {
        qWarning() << Q_FUNC_INFO << "browser is null";
    }
}
