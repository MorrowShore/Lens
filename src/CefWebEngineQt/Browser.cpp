#include "Browser.h"
#include "Manager.h"

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

void Browser::close()
{
    //TODO
}

}
