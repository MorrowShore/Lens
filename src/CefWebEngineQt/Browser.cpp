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

void Browser::close()
{
    manager.closeBrowser(id);
}

}
