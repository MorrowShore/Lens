#include "QmlUtils.h"
#include "QtMiscUtils.h"
#include "log/loghandler.h"
#include <QProcess>
#include <QApplication>
#include <QQmlEngine>
#include <QSysInfo>
#include <QWindow>
#include <QDesktopServices>

#ifdef Q_OS_WINDOWS
#include <windows.h>
#include <wininet.h>
#endif

namespace
{

static const QString SK_EnabledHardwareGraphicsAccelerator = "enabledHardwareGraphicsAccelerator";
static const QString SK_EnabledHighDpiScaling = "enabledHighDpiScaling";

}

QmlUtils* QmlUtils::_instance = nullptr;

QmlUtils::QmlUtils(QSettings& settings_, const QString& settingsGroup, QObject *parent)
    : QObject(parent)
    , settings(settings_)
    , SettingsGroupPath(settingsGroup)
{
    if (_instance)
    {
        qWarning() << "instance already initialized";
    }

    _instance = this;

    {
        if (enabledHardwareGraphicsAccelerator())
        {
            //AA_UseDesktopOpenGL == OpenGL для ПК (работает плохо)
            //AA_UseOpenGLES == OpenGL для мобильных платформ (работает лучше всего)
            //ToDo: неизвестно, что произойдёт, если не будет поддерживаться аппаратное ускорение устройством
            QCoreApplication::setAttribute(Qt::AA_UseOpenGLES);
        }
        else
        {
            QCoreApplication::setAttribute(Qt::AA_UseSoftwareOpenGL);
        }
    }

    {
        if (enabledHighDpiScaling())
        {
            QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
        }
        else
        {
            QApplication::setAttribute(Qt::AA_DisableHighDpiScaling);
        }
    }
}

void QmlUtils::restartApplication()
{
    QProcess::startDetached(qApp->arguments()[0], qApp->arguments());
    QtMiscUtils::quitDeferred();
}

void QmlUtils::openLogDirectory()
{
    LogHandler::openDirectory();
}

bool QmlUtils::enabledHardwareGraphicsAccelerator() const
{
    return settings.value(SettingsGroupPath + "/" + SK_EnabledHardwareGraphicsAccelerator, true).toBool();
}

void QmlUtils::setEnabledHardwareGraphicsAccelerator(bool enabled)
{
    if (enabled != enabledHardwareGraphicsAccelerator())
    {
        settings.setValue(SettingsGroupPath + "/" + SK_EnabledHardwareGraphicsAccelerator, enabled);
        emit dataChanged();
    }
}

bool QmlUtils::enabledHighDpiScaling() const
{
    return settings.value(SettingsGroupPath + "/" + SK_EnabledHighDpiScaling, false).toBool();
}

void QmlUtils::setEnabledHighDpiScaling(bool enabled)
{
    if (enabled != enabledHighDpiScaling())
    {
        settings.setValue(SettingsGroupPath + "/" + SK_EnabledHighDpiScaling, enabled);
        emit dataChanged();
    }
}

QString QmlUtils::buildCpuArchitecture() const
{
    return QSysInfo::buildCpuArchitecture();
}

void QmlUtils::setValue(const QString &key, const QVariant &value)
{
    return settings.setValue(SettingsGroupPath + "/" + key, value);
}

bool QmlUtils::valueBool(const QString& key, bool defaultValue)
{
    return settings.value(SettingsGroupPath + "/" + key, defaultValue).toBool();
}

qreal QmlUtils::valueReal(const QString &key, qreal defaultValue)
{
    bool ok = false;
    const qreal result = settings.value(SettingsGroupPath + "/" + key, defaultValue).toReal(&ok);
    if (ok)
    {
        return result;
    }

    return defaultValue;
}

void QmlUtils::updateWindowStyle(QWindow* window) const
{
    if (!window)
    {
        return;
    }

    QtMiscUtils::setDarkWindowFrame(window->winId());
}

void QmlUtils::declareQml()
{
    qmlRegisterUncreatableType<QmlUtils> ("AxelChat.QmlUtils",
                                         1, 0, "QmlUtils", "Type cannot be created in QML");
}

QmlUtils *QmlUtils::instance()
{
    if (!_instance)
    {
        qWarning() << "instance not initialized";
    }

    return _instance;
}

void QmlUtils::setQmlApplicationEngine(const QQmlApplicationEngine* qmlEngine_)
{
    qmlEngine = qmlEngine_;
}
