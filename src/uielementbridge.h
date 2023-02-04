#pragma once

#include "setting.h"
#include <QString>
#include <QQmlEngine>
#include <QQuickItem>
#include <set>

class UIElementBridge : public QObject
{
    Q_OBJECT

public:
    enum class Type
    {
        Unknown = -1,
        Label = 10,
        LineEdit = 20,
        Button = 30,
        Switch = 32,
    };

    static UIElementBridge* createLineEdit(Setting<QString>* setting, const QString& name, const QString& placeHolder = QString(), const bool passwordEcho = false);
    static UIElementBridge* createButton(const QString& text, std::function<void()> invokeCallback);
    static UIElementBridge* createLabel(const QString& text);
    static UIElementBridge* createSwitch(Setting<bool>* settingBool, const QString& name);

    Q_INVOKABLE void bindQmlItem(QQuickItem* item);

    Setting<QString>* getSettingString() { return settingString; }
    const Setting<QString>* getSettingString() const { return settingString; }

    Setting<bool>* getSettingBool() { return settingBool; }
    const Setting<bool>* getSettingBool() const { return settingBool; }

    Q_INVOKABLE int getTypeInt() const { return (int)type; }

    bool executeInvokeCallback();

    void setItemProperty(const QByteArray& name, const QVariant& value);

#ifdef QT_QUICK_LIB
    static void declareQml()
    {
        qmlRegisterUncreatableType<UIElementBridge> ("AxelChat.UIElementBridge", 1, 0, "UIElementBridge", "Type cannot be created in QML");
    }
#endif

signals:
    void valueChanged();

protected:
    void updateItemProperties();

private slots:
    void onInvoked();
    void onTextChanged();
    void onCheckedChanged();

private:
    UIElementBridge(){}

    QQuickItem* item = nullptr;

    Setting<QString>* settingString;
    Setting<bool>* settingBool;

    Type type;
    QMap<QByteArray, QVariant> parameters;
    std::function<void()> invokeCallback;
};