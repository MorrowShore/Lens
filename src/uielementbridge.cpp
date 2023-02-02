#include "uielementbridge.h"

UIElementBridge* UIElementBridge::createLineEdit(Setting<QString> *setting, const QString &name, const QString &placeHolder, const std::set<Flag> &additionalFlags)
{
    UIElementBridge* element = new UIElementBridge();

    element->type = Type::LineEdit;
    element->settingString = setting;
    element->name = name;
    element->placeHolder = placeHolder;
    element->flags.insert(additionalFlags.begin(), additionalFlags.end());

    return element;
}

UIElementBridge* UIElementBridge::createButton(const QString &text, std::function<void (const QVariant &)> invokeCalback, const QVariant &invokeCallbackArgument_)
{
    UIElementBridge* element = new UIElementBridge();

    element->type = Type::Button;
    element->name = text;
    element->invokeCalback = invokeCalback;
    element->invokeCallbackArgument = invokeCallbackArgument_;

    return element;
}

UIElementBridge* UIElementBridge::createLabel(const QString &text)
{
    UIElementBridge* element = new UIElementBridge();

    element->type = Type::Label;
    element->name = text;

    return element;
}

UIElementBridge* UIElementBridge::createSwitch(Setting<bool> *settingBool, const QString &name)
{
    UIElementBridge* element = new UIElementBridge();

    element->type = Type::Switch;
    element->settingBool = settingBool;
    element->name = name;

    return element;
}

bool UIElementBridge::executeInvokeCallback()
{
    if (!invokeCalback)
    {
        qCritical() << Q_FUNC_INFO << "invoke callback not setted";
        return false;
    }

    invokeCalback(invokeCallbackArgument);

    return true;
}
