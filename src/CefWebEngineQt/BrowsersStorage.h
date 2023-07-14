#pragma once

#include "Browser.h"
#include <QDebug>
#include <set>

namespace cweqt
{

class BrowsersStorage
{
public:
    void addWithBrowserId(std::shared_ptr<Browser> browser, int browserId)
    {
        if (byBrowserId.find(browserId) != byBrowserId.end())
        {
            qWarning() << Q_FUNC_INFO << "browser id" << browserId << "already exists";
        }

        byBrowserId[browserId] = browser;
    }

    void addWithMessageId(std::shared_ptr<Browser> browser, int64_t messageId)
    {
        if (byMessageId.find(messageId) != byMessageId.end())
        {
            qWarning() << Q_FUNC_INFO << "message id" << messageId << "already exists";
        }

        byMessageId[messageId] = browser;
    }

    void moveFromMessageIdToBrowserId(int64_t messageId, int browserId)
    {
        auto it = byMessageId.find(messageId);

        if (it == byMessageId.end())
        {
            qWarning() << Q_FUNC_INFO << "message id" << messageId << "not found";
            return;
        }

        std::shared_ptr<Browser> browser = it->second;
        byMessageId.erase(messageId);
        addWithBrowserId(browser, browserId);
    }

    void removeByBrowserId(const int id)
    {
        byBrowserId.erase(id);
    }

    std::shared_ptr<Browser> findByBrowserId(int browserId) const
    {
        auto it = byBrowserId.find(browserId);
        if (it == byBrowserId.end())
        {
            return nullptr;
        }

        return it->second;
    }

    std::shared_ptr<Browser> findByMessageId(int64_t messageId) const
    {
        auto it = byMessageId.find(messageId);
        if (it == byMessageId.end())
        {
            return nullptr;
        }

        return it->second;
    }

    void clear()
    {
        for (const auto &[id, browser] : byBrowserId)
        {
            browser->close();
        }

        byBrowserId.clear();

        for (const auto &[id, browser] : byMessageId)
        {
            browser->close();
        }

        byMessageId.clear();
    }

private:
    std::map<int, std::shared_ptr<Browser>> byBrowserId;
    std::map<int64_t, std::shared_ptr<Browser>> byMessageId;
};

}
