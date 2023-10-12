#include "uibridgeelement.h"
#include <QAction>

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
        qCritical() << "item is null";
        return;
    }

    QObject::connect(item_, &QQuickItem::destroyed, this, [this]()
    {
        if (sender() == item)
        {
            item = nullptr;
        }
    });

    updateItemProperties();

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

    if (type == Type::Slider)
    {
        QObject::connect(item, SIGNAL(valueChanged()), this, SLOT(onValueChanged()));
    }

    if (type == Type::ComboBox)
    {
        QObject::connect(item, SIGNAL(currentIndexChanged()), this, SLOT(onCurrentIndexChanged()));
    }
}

void UIBridgeElement::bindAction(QAction *action_)
{
    action = action_;

    if (!action)
    {
        qCritical() << "action is null";
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
            qWarning() << "setting of bool is null";
        }
    }
    else
    {
        qWarning() << "incompatible with this type";
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
        qCritical() << "invoke callback is null";
    }
}

void UIBridgeElement::onTextChanged()
{
    QObject* sender_ = sender();
    if (!sender_)
    {
        qCritical() << "sender is null";
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
        qWarning() << "setting is null";
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
        qCritical() << "sender is null";
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
        qWarning() << "unknown sender" << sender_;
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
        qWarning() << "setting is null";
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

void UIBridgeElement::onValueChanged()
{
    QObject* sender_ = sender();
    if (!sender_)
    {
        qCritical() << "sender is null";
        return;
    }

    bool changed = false;

    const double value = sender_->property("value").toDouble();

    if (value != parameters.value("value"))
    {
        changed = true;
    }

    parameters.insert("value", value);

    if (settingDouble)
    {
        if (value != settingDouble->get())
        {
            changed = true;
        }

        settingDouble->set(value);
    }
    else
    {
        qWarning() << "setting is null";
    }

    if (changed)
    {
        emit valueChanged();
    }
}

void UIBridgeElement::onCurrentIndexChanged()
{
    QObject* sender_ = sender();
    if (!sender_)
    {
        qCritical() << "sender is null";
        return;
    }

    bool changed = false;

    const int value = sender_->property("currentIndex").toInt();

    qDebug() << value;

    if (value != parameters.value("currentIndex"))
    {
        changed = true;
    }

    parameters.insert("currentIndex", value);

    if (settingInt)
    {
        if (value != settingInt->get())
        {
            changed = true;
        }

        settingInt->set(value);

        qDebug() << settingInt->getKey() << settingInt->get() << "OK";
    }
    else
    {
        qWarning() << "setting is null";
    }

    if (changed)
    {
        emit valueChanged();
    }
}
