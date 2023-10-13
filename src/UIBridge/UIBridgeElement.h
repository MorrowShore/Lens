#pragma once

#include "setting.h"
#include <QString>
#include <QQmlEngine>
#include <QQuickItem>
#include <set>

class UIBridgeElement : public QObject
{
    Q_OBJECT

public:
    friend class UIBridge;

    enum class Type
    {
        Unknown     = -1,
        Label       = 10,
        LineEdit    = 20,
        Button      = 30,
        Switch      = 32,
        Slider      = 40,
        ComboBox    = 50,
    };

    explicit UIBridgeElement(QObject *parent = nullptr);

    Q_INVOKABLE void bindQuickItem(QQuickItem* item);

    void bindAction(QAction* action);

    Setting<QString>* getSettingString() { return settingString; }
    const Setting<QString>* getSettingString() const { return settingString; }

    Setting<bool>* getSettingBool() { return settingBool; }
    const Setting<bool>* getSettingBool() const { return settingBool; }

    Setting<double>* getSettingDouble() { return settingDouble; }
    const Setting<double>* getSettingDouble() const { return settingDouble; }

    Setting<int>* getSettingInt() { return settingInt; }
    const Setting<int>* getSettingInt() const { return settingInt; }

    Q_INVOKABLE int getTypeInt() const { return (int)type; }

    bool executeInvokeCallback();

    void setItemProperty(const QByteArray& name, const QVariant& value);

    void setVisible(const bool visible);

#ifdef QT_QUICK_LIB
    static void declareQml()
    {
        qmlRegisterUncreatableType<UIBridgeElement> ("AxelChat.UIBridgeElement", 1, 0, "UIBridgeElement", "Type cannot be created in QML");
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
    void onValueChanged();
    void onCurrentIndexChanged();

private:
    QQuickItem* item = nullptr;

    Setting<QString>* settingString;
    Setting<bool>* settingBool;
    Setting<double>* settingDouble;
    Setting<int>* settingInt;

    QAction* action = nullptr;

    Type type;
    QMap<QByteArray, QVariant> parameters;
    std::function<void()> invokeCallback;
};
