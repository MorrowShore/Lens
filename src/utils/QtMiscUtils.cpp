#include "QtMiscUtils.h"
#include <QCoreApplication>
#include <QTimer>
#include <QDebug>

#ifdef Q_OS_WINDOWS
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#include <dwmapi.h>
#endif

namespace
{

static std::function<void()> beforeQuitDeferredCallback = nullptr;
static int timeBeforeQuitDeferred = 1000;

}

#ifdef Q_OS_WINDOWS
void QtMiscUtils::setDarkWindowFrame(const WId wid)
{
    BOOL value = TRUE;
    ::DwmSetWindowAttribute((HWND)wid, DWMWA_USE_IMMERSIVE_DARK_MODE, &value, sizeof(value));
}
#endif

void QtMiscUtils::setBeforeQuitDeferred(std::function<void()> callback, const int timeBeforeQuitDeferred_)
{
    beforeQuitDeferredCallback = callback;
    timeBeforeQuitDeferred = timeBeforeQuitDeferred_;
}

void QtMiscUtils::quitDeferred()
{
    if (beforeQuitDeferredCallback)
    {
        beforeQuitDeferredCallback();
    }
    else
    {
        qWarning() << "callback is null";
    }

    QTimer::singleShot(timeBeforeQuitDeferred, &QCoreApplication::quit);
}
