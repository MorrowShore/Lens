#include "rumble.h"
#include <QNetworkRequest>
#include <QNetworkReply>

namespace
{

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

    return true;
}

}

Rumble::Rumble(QSettings& settings, const QString& settingsGroupPath, QNetworkAccessManager& network_, QObject *parent)
    : ChatService(settings, settingsGroupPath, AxelChat::ServiceType::Rumble, parent)
    , network(network_)
{
    // https://wn0.rumble.com/service.php?video=2rhh6c&name=video.watching_now
    // https://rumble.com/chat/popup/167097396

    getUIElementBridgeBySetting(stream)->setItemProperty("name", tr("Stream"));
    getUIElementBridgeBySetting(stream)->setItemProperty("placeholderText", tr("Stream link..."));

    reconnect();
}

ChatService::ConnectionStateType Rumble::getConnectionStateType() const
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

QString Rumble::getStateDescription() const
{
    switch (getConnectionStateType())
    {
    case ConnectionStateType::NotConnected:
        if (stream.get().isEmpty())
        {
            return tr("Channel not specified");
        }

        if (state.streamId.isEmpty())
        {
            return tr("The channel is not correct");
        }

        return tr("Not connected");

    case ConnectionStateType::Connecting:
        return tr("Connecting...");

    case ConnectionStateType::Connected:
        return tr("Successfully connected!");

    }

    return "<unknown_state>";
}

void Rumble::reconnectImpl()
{
    state = State();
    info = Info();

    state.streamId = extractLinkId(stream.get());

    if (state.streamId.isEmpty())
    {
        emit stateChanged();
        return;
    }

    state.streamUrl = QUrl("https://rumble.com/" + state.streamId + ".html");

    if (!enabled.get())
    {
        return;
    }

    requestVideoPage();
}

QString Rumble::extractLinkId(const QString &rawLink)
{
    QString result = rawLink.trimmed();

    if (rawLink.startsWith("http", Qt::CaseSensitivity::CaseInsensitive) || rawLink.startsWith("rumble.com/", Qt::CaseSensitivity::CaseInsensitive))
    {
        result = AxelChat::simplifyUrl(rawLink);
    }

    result = AxelChat::removeFromStart(result, "rumble.com/", Qt::CaseSensitivity::CaseInsensitive);
    result = AxelChat::removeFromEnd(result, ".html", Qt::CaseSensitivity::CaseInsensitive);

    if (result.contains('/'))
    {
        return QString();
    }

    return result.trimmed();
}

void Rumble::requestVideoPage()
{
    if (!enabled.get() || state.streamUrl.isEmpty())
    {
        return;
    }

    QNetworkRequest request(state.streamUrl);
    QNetworkReply* reply = network.get(request);
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]()
    {
        QByteArray data;
        if (!checkReply(reply, Q_FUNC_INFO, data))
        {
            return;
        }

        const QString chatId = parseChatId(data);
        if (chatId.isEmpty())
        {
            qWarning() << Q_FUNC_INFO << "failed to parse chat id";
            return;
        }

        info.chatId = chatId;
        state.chatUrl = "https://rumble.com/chat/popup/" + chatId;

        emit stateChanged();

        requestChatPage();
    });
}

void Rumble::requestChatPage()
{
    if (!enabled.get() || info.chatId.isEmpty())
    {
        return;
    }


}

QString Rumble::parseChatId(const QByteArray &html)
{
    static const QByteArray Prefix = "class=\"rumbles-vote rumbles-vote-with-bar\" data-type=\"1\" data-id=\"";

    const int prefixStartPos = html.indexOf(Prefix);
    if (prefixStartPos == -1)
    {
        qWarning() << "failed to find prefix";
        return QString();
    }

    const int resultStartPos = prefixStartPos + Prefix.length();

    int resultLastPos = -1;
    for (int i = resultStartPos; i < std::min(html.length(), resultStartPos + 128); ++i)
    {
        const QChar& c = html[i];
        if (c == '"')
        {
            resultLastPos = i;
            break;
        }
    }

    if (resultLastPos == -1)
    {
        qDebug() << Q_FUNC_INFO << "not found '\"'";
        return QString();
    }

    return QString::fromUtf8(html.mid(resultStartPos, resultLastPos - resultStartPos));
}
