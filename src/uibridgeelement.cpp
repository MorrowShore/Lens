#include "uibridgeelement.h"
#include <QAction>

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

UIBridgeElement::UIBridgeElement(QObject *parent)
    : QObject(parent)
{

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
        if (sender() == item)
        {
            item = nullptr;
        }
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

void UIBridgeElement::bindAction(QAction *action_)
{
    action = action_;

    if (!action)
    {
        qCritical() << Q_FUNC_INFO << "action is null";
        return;
    }

    connect(action, &QAction::destroyed, this, [this]()
    {
        if (sender() == action)
        {
            action = nullptr;
        }
    });

    if (action->text().isEmpty())
    {
        action->setText(parameters.value("text").toString());
    }

    if (type == Type::Switch)
    {
        action->setCheckable(true);

        if (settingBool)
        {
            action->setChecked(settingBool->get());

            connect(action, &QAction::triggered, this, &UIBridgeElement::onCheckedChanged);
        }
        else
        {
            qWarning() << Q_FUNC_INFO << "setting of bool is null";
        }
    }
    else
    {
        qWarning() << Q_FUNC_INFO << "incompatible with this type";
    }
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
        qCritical() << Q_FUNC_INFO << "sender is null";
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
        qWarning() << Q_FUNC_INFO << "setting is null";
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
        qCritical() << Q_FUNC_INFO << "sender is null";
        return;
    }

    QQuickItem* itemSender = dynamic_cast<QQuickItem*>(sender_);
    QAction* actionSender = dynamic_cast<QAction*>(sender_);

    bool checked = false;

    if (itemSender)
    {
        checked = itemSender->property("checked").toBool();
    }
    else if (actionSender)
    {
        checked = actionSender->isChecked();
    }
    else
    {
        qWarning() << Q_FUNC_INFO << "unknown sender" << sender_;
        return;
    }

    bool changed = false;

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
        qWarning() << Q_FUNC_INFO << "setting is null";
    }

    if (itemSender)
    {
        if (action)
        {
            action->setChecked(checked);
        }
    }
    else if (actionSender)
    {
        if (item)
        {
            updateItemProperties();
        }
    }

    if (changed)
    {
        emit valueChanged();
    }
}
