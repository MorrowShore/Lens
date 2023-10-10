#pragma once

#include <QObject>
#include <QSettings>
#include <QQmlApplicationEngine>
#include <QWindow>

class QmlUtils : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool enabledHardwareGraphicsAccelerator READ enabledHardwareGraphicsAccelerator WRITE setEnabledHardwareGraphicsAccelerator NOTIFY dataChanged)
    Q_PROPERTY(bool enabledHighDpiScaling READ enabledHighDpiScaling WRITE setEnabledHighDpiScaling NOTIFY dataChanged)
    Q_PROPERTY(QString buildCpuArchitecture READ buildCpuArchitecture CONSTANT)

public:
    static void declareQml();
    static QmlUtils* instance();

    explicit QmlUtils(QSettings& settings, const QString& settingsGroup, QObject *parent = nullptr);

    void setQmlApplicationEngine(const QQmlApplicationEngine* qmlEngine);

    Q_INVOKABLE static void restartApplication();
    Q_INVOKABLE static void openLogDirectory();

    bool enabledHardwareGraphicsAccelerator() const;
    void setEnabledHardwareGraphicsAccelerator(bool enabled);

    bool enabledHighDpiScaling() const;
    void setEnabledHighDpiScaling(bool enabled);

    QString buildCpuArchitecture() const;

    Q_INVOKABLE void setValue(const QString& key, const QVariant& value);

    Q_INVOKABLE bool valueBool(const QString& key, bool defaultValue);
    Q_INVOKABLE qreal valueReal(const QString& key, qreal defaultValue);

    Q_INVOKABLE void updateWindowStyle(QWindow* window) const;

signals:
    void dataChanged();
    void triggered(const QString& action);

private:
    static QmlUtils* _instance;

    QSettings& settings;
    const QString SettingsGroupPath = "qml_utils";
    const QQmlApplicationEngine* qmlEngine = nullptr;
};
