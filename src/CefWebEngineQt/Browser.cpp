#include "Browser.h"
#include "Manager.h"
#include <QDebug>

namespace cweqt
{

Browser::Browser(Manager& manager_, const int id_, const QUrl& initialUrl_, const bool opened_, QObject *parent)
    : QObject{parent}
    , manager(manager_)
    , id(id_)
    , initialUrl(initialUrl_)
    , opened(opened_)
{

}

void Browser::setClosed()
{
    if (!opened)
    {
        return;
    }

    opened = false;
    emit closed();
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

void Browser::addResponseData(const uint64_t responseId, const QByteArray &data)
{
    auto it = responses.find(responseId);
    if (it == responses.end() || !it->second)
    {
        qWarning() << Q_FUNC_INFO << "response id" << responseId << "not found or null, browser id =" << id;
        return;
    }

    it->second->data += data;
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

    response->data = QByteArray::fromBase64(response->data);

    emit recieved(response);
}

void Browser::close()
{
    manager.closeBrowser(id);
}

}
