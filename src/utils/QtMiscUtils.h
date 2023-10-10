#pragma once

#include <qwindowdefs.h>
#include <functional>

class QtMiscUtils
{
public:
#ifdef Q_OS_WINDOWS
    static void setDarkWindowFrame(const WId wid);
#endif

    static void setBeforeQuitDeferred(std::function<void()> callback, const int timeBeforeQuitDeferred);
    static void quitDeferred();

private:
    QtMiscUtils(){}
};
