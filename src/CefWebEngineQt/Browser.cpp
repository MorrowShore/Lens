#include "Browser.h"
#include "Manager.h"
#include <QDebug>

namespace cweqt
{

Browser::Browser(Manager& manager_, const int id_, const QUrl& initialUrl_, const bool opened)
    : manager(manager_)
    , id(id_)
    , initialUrl(initialUrl_)
    , state(opened ? State::Opened : State::NotOpened)
{

}

void Browser::setOpened(const int id_)
{
    if (state == State::Opened)
    {
        qWarning() << Q_FUNC_INFO << "browser" << id << "already opened";
        return;
    }

    if (state == State::Closed)
    {
        qWarning() << Q_FUNC_INFO << "browser" << id << "once closed and can't be opened again";
        return;
    }

    state = State::Opened;
    const_cast<int&>(id) = id_;

    emit opened();
}

void Browser::setClosed()
{
    if (state == State::Closed)
    {
        qWarning() << "browser" << id << "already closed";
        return;
    }

    state = State::Closed;

    emit closed();
}

void Browser::setWindowHandle(const uint64_t handle)
{
    window = QWindow::fromWinId((WId)handle);

    if (!window && handle != 0)
    {
        qWarning() << Q_FUNC_INFO << "failed to get window from handle" << handle;
    }
}

void Browser::registerResponse(const std::shared_ptr<Response>& response)
{
    if (!response)
    {
        qWarning() << Q_FUNC_INFO << "response is null";
        return;
    }

    responses[response->requestId] = response;
}

void Browser::addResponseData(const uint64_t responseId, const QByteArray &data, const int64_t)
{
    auto it = responses.find(responseId);
    if (it == responses.end() || !it->second)
    {
        qWarning() << Q_FUNC_INFO << "response id" << responseId << "not found or null, browser id =" << id;
        return;
    }

    if (!it->second->validData)
    {
        return;
    }

    const QByteArray::FromBase64Result result = QByteArray::fromBase64Encoding(data, QByteArray::Base64Option::AbortOnBase64DecodingErrors);
    if (result.decodingStatus == QByteArray::Base64DecodingStatus::Ok)
    {
        it->second->data += result.decoded;
    }
    else
    {
        it->second->data.clear();
        it->second->validData = false;
        qWarning() << Q_FUNC_INFO << "failed to parse base64 data, status =" << (int)result.decodingStatus << ", base64 data[" << data.length() << "] =" << data;
    }
}

void Browser::finalizeResponse(const uint64_t responseId)
{
    auto it = responses.find(responseId);
    if (it == responses.end() || !it->second)
    {
        qWarning() << Q_FUNC_INFO << "response id" << responseId << "not found or null, browser id =" << id;
        return;
    }

    std::shared_ptr<Response> response = it->second;
    responses.erase(it);

    emit recieved(response);
}

void Browser::close()
{
    manager.closeBrowser(id);
}

Browser::Settings Browser::getDefaultSettingsForDisposable()
{
    Settings settings;

    settings.showResponses = true;
    settings.visible = false;

    return settings;
}

}
