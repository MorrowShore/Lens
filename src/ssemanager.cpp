#include "ssemanager.h"

SseManager::SseManager(QNetworkAccessManager& network_, QObject *parent)
    : QObject{parent}
    , network(network_)
{

}

SseManager::~SseManager()
{
    stop();
}

void SseManager::start(const QUrl &url)
{
    stop();

    QNetworkRequest request(url);
    request.setRawHeader("Accept", "text/event-stream");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);

    reply = network.get(request);
    QObject::connect(reply, &QNetworkReply::readyRead, this, [this]()
    {
        while (reply->canReadLine())
        {
            if (!isStarted)
            {
                isStarted = true;
                emit started();
            }

            const QByteArray message = reply->readLine().trimmed();

            int startPos = 0;
            if (message.startsWith("data:"))
            {
                startPos = 5;
            }

            const QByteArray data = message.mid(startPos).trimmed();

            if (!data.isEmpty())
            {
                emit readyRead(data);
            }
        }
    });

    QObject::connect(reply, &QNetworkReply::finished, this, [this]()
    {
        stop();
    });
}

void SseManager::stop()
{
    isStarted = false;

    if (!reply)
    {
        return;
    }

    reply->deleteLater();
    reply = nullptr;

    emit stopped();
}
