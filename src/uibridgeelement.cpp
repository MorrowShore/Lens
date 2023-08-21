#include "uibridgeelement.h"

UIBridgeElement* UIBridgeElement::createLineEdit(Setting<QString> *setting, const QString &name, const QString &placeHolder, const bool passwordEcho)
{
    UIBridgeElement* element = new UIBridgeElement();

    element->type = Type::LineEdit;
    element->settingString = setting;

    element->setItemProperty("name", name);
    element->setItemProperty("placeholderText", placeHolder);

    element->setItemProperty("passwordEcho", passwordEcho);

    if (setting)
    {
        element->setItemProperty("text", setting->get());
    }
    else
    {
        qWarning() << Q_FUNC_INFO << "!setting";
    }

    element->updateItemProperties();

    return element;
}

UIBridgeElement* UIBridgeElement::createButton(const QString &text, std::function<void()> invokeCallback)
{
    UIBridgeElement* element = new UIBridgeElement();

    element->type = Type::Button;

    element->invokeCallback = invokeCallback;
    element->setItemProperty("text", text);

    element->updateItemProperties();

    return element;
}

UIBridgeElement* UIBridgeElement::createLabel(const QString &text)
{
    UIBridgeElement* element = new UIBridgeElement();

    element->type = Type::Label;
    element->setItemProperty("text", text);

    element->updateItemProperties();

    return element;
}

UIBridgeElement* UIBridgeElement::createSwitch(Setting<bool> *settingBool, const QString &name)
{
    UIBridgeElement* element = new UIBridgeElement();

    element->type = Type::Switch;
    element->settingBool = settingBool;
    element->setItemProperty("text", name);

    if (settingBool)
    {
        element->setItemProperty("checked", settingBool->get());
    }

    element->updateItemProperties();

    return element;
}

UIBridgeElement *UIBridgeElement::createComboBox(Setting<int> *settingInt, const QString &name, const QList<QPair<int, QString>>& values, std::function<void()> valueChanged)
{
    UIBridgeElement* element = new UIBridgeElement();

    element->type = Type::ComboBox;
    element->settingInt = settingInt;
    element->setItemProperty("text", name);
    element->comboBoxValues = values;
    element->invokeCallback = valueChanged;

    element->updateItemProperties();

    return element;
}

void UIBridgeElement::bindQuickItem(QQuickItem *item_)
{
    if (item)
    {
        QObject::disconnect(item);
        item = nullptr;
    }

    item = item_;
    if (!item_)
    {
        qCritical() << Q_FUNC_INFO << "item is null";
        return;
    }

    QObject::connect(item_, &QQuickItem::destroyed, this, [this]()
    {
        item = nullptr;
    });

    if (type == Type::Button)
    {
        QObject::connect(item, SIGNAL(clicked()), this, SLOT(onInvoked()));
    }

    if (type == Type::LineEdit)
    {
        QObject::connect(item, SIGNAL(textChanged()), this, SLOT(onTextChanged()));
    }

    if (type == Type::Switch)
    {
        QObject::connect(item, SIGNAL(checkedChanged()), this, SLOT(onCheckedChanged()));
    }

    updateItemProperties();
}

void UIBridgeElement::setItemProperty(const QByteArray &name, const QVariant &value)
{
    parameters.insert(name, value);

    if (item)
    {
        item->setProperty(name, value);
    }
}

void UIBridgeElement::updateItemProperties()
{
    if (!item)
    {
        return;
    }

    for (auto it = parameters.begin(); it != parameters.end(); it++)
    {
        item->setProperty(it.key(), it.value());
    }
}

void UIBridgeElement::onInvoked()
{
    if (invokeCallback)
    {
        invokeCallback();
    }
    else
    {
        qCritical() << Q_FUNC_INFO << "invoke callback is null";
    }
}

void UIBridgeElement::onTextChanged()
{
    QObject* sender_ = sender();
    if (!sender_)
    {
        qCritical() << Q_FUNC_INFO << "!sender_";
        return;
    }

    bool changed = false;

    const QString text = sender_->property("text").toString();

    if (text != parameters.value("text"))
    {
        changed = true;
    }

    parameters.insert("text", text);

    if (settingString)
    {
        if (text != settingString->get())
        {
            changed = true;
        }

        settingString->set(text);
    }
    else
    {
        qWarning() << Q_FUNC_INFO << "!setting";
    }

    if (changed)
    {
        emit valueChanged();
    }
}

void UIBridgeElement::onCheckedChanged()
{
    QObject* sender_ = sender();
    if (!sender_)
    {
        qCritical() << Q_FUNC_INFO << "!sender_";
        return;
    }

    bool changed = false;

    const bool checked = sender_->property("checked").toBool();

    if (checked != parameters.value("checked"))
    {
        changed = true;
    }

    parameters.insert("checked", checked);

    if (settingBool)
    {
        if (checked != settingBool->get())
        {
            changed = true;
        }

        settingBool->set(checked);
    }
    else
    {
        qWarning() << Q_FUNC_INFO << "!setting";
    }

    if (changed)
    {
        emit valueChanged();
    }
}
