#pragma once

#include "setting.h"
#include <QString>
#include <set>

class UIElementBridge : public QObject
{
    Q_OBJECT

public:
    enum class Type
    {
        Unknown = -1,
        LineEdit = 10,
        Button = 20,
        Label = 21,
        Switch = 22,
    };

    enum class Flag
    {
        Visible = 1,
        PasswordEcho = 10,
    };

    static UIElementBridge* createLineEdit(Setting<QString>* setting, const QString& name, const QString& placeHolder = QString(), const std::set<Flag>& additionalFlags = std::set<Flag>());
    static UIElementBridge* createButton(const QString& text, std::function<void(const QVariant& argument)> invokeCalback, const QVariant& invokeCallbackArgument_ = QVariant());
    static UIElementBridge* createLabel(const QString& text);
    static UIElementBridge* createSwitch(Setting<bool>* settingBool, const QString& name);

    Setting<QString>* getSettingString() { return settingString; }
    const Setting<QString>* getSettingString() const { return settingString; }

    Setting<bool>* getSettingBool() { return settingBool; }
    const Setting<bool>* getSettingBool() const { return settingBool; }

    QString getName() const { return name; }
    void setName(const QString& name_) { name = name_; }
    Type getType() const { return type; }
    const std::set<Flag>& getFlags() const { return flags; }
    void setFlag(const Flag flag) { flags.insert(flag); }
    void resetFlag(const Flag flag) { flags.erase(flag); }

    void setPlaceholder(const QString& placeHolder_) { placeHolder = placeHolder_; }
    QString getPlaceholder() const { return placeHolder; }

    bool executeInvokeCallback();

private:
    explicit UIElementBridge(){}

    Setting<QString>* settingString;
    Setting<bool>* settingBool;

    QString name;
    Type type;
    std::set<Flag> flags = { Flag::Visible };
    QString placeHolder;
    std::function<void(const QVariant& argument)> invokeCalback;
    QVariant invokeCallbackArgument;
};
