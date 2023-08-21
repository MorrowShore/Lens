#include "uibridge.h"

UIBridge::UIBridge(QObject *parent)
    : QObject{parent}
{
    QQmlEngine::setObjectOwnership(this, QQmlEngine::CppOwnership);
}

std::shared_ptr<UIBridgeElement> UIBridge::addLabel(const QString &text)
{
    std::shared_ptr<UIBridgeElement> element = std::make_shared<UIBridgeElement>();

    element->type = UIBridgeElement::Type::Label;
    element->setItemProperty("text", text);

    element->updateItemProperties();

    addElement(element);

    return element;
}

std::shared_ptr<UIBridgeElement> UIBridge::addButton(const QString &text, std::function<void ()> invokeCallback)
{
    std::shared_ptr<UIBridgeElement> element = std::make_shared<UIBridgeElement>();

    element->type = UIBridgeElement::Type::Button;

    element->invokeCallback = invokeCallback;
    element->setItemProperty("text", text);

    element->updateItemProperties();

    addElement(element);

    return element;
}

std::shared_ptr<UIBridgeElement> UIBridge::addLineEdit(Setting<QString> *setting, const QString &name, const QString &placeHolder, const bool passwordEcho)
{
    std::shared_ptr<UIBridgeElement> element = std::make_shared<UIBridgeElement>();

    element->type = UIBridgeElement::Type::LineEdit;
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
        qWarning() << Q_FUNC_INFO << "setting is null";
    }

    element->updateItemProperties();

    addElement(element);

    return element;
}

std::shared_ptr<UIBridgeElement> UIBridge::addSwitch(Setting<bool> *settingBool, const QString &name)
{
    std::shared_ptr<UIBridgeElement> element = std::make_shared<UIBridgeElement>();

    element->type = UIBridgeElement::Type::Switch;
    element->settingBool = settingBool;
    element->setItemProperty("text", name);

    if (settingBool)
    {
        element->setItemProperty("checked", settingBool->get());
    }

    element->updateItemProperties();

    addElement(element);

    return element;
}

std::shared_ptr<UIBridgeElement> UIBridge::addSlider(Setting<int> *settingInt, const QString &name, const int minValue, const int maxValue, const bool valueShowAsPercent)
{
    std::shared_ptr<UIBridgeElement> element = std::make_shared<UIBridgeElement>();

    element->type = UIBridgeElement::Type::Slider;
    element->settingInt = settingInt;

    element->setItemProperty("name", name);
    element->setItemProperty("from", minValue);
    element->setItemProperty("to", maxValue);
    element->setItemProperty("defaultValue", settingInt->getDefaultValue());
    element->setItemProperty("value", settingInt->get());
    element->setItemProperty("valueShowAsPercent", valueShowAsPercent);

    element->updateItemProperties();

    addElement(element);

    return element;
}

void UIBridge::addElement(const std::shared_ptr<UIBridgeElement> &element)
{
    if (!element)
    {
        qCritical() << Q_FUNC_INFO << "element is null";
        return;
    }

    connect(element.get(), &UIBridgeElement::valueChanged, this, [this, element]()
    {
        emit elementChanged(element);
    });

    elements.append(element);
}

int UIBridge::getElementsCount() const
{
    return elements.count();
}

int UIBridge::getElementType(const int index) const
{
    if (index < 0 || index >= elements.count())
    {
        qCritical() << Q_FUNC_INFO << "invalid index" << index;
        return -1;
    }

    return elements[index]->getTypeInt();
}

void UIBridge::bindQuickItem(const int index, QQuickItem *item)
{
    if (index < 0 || index >= elements.count())
    {
        qCritical() << Q_FUNC_INFO << "invalid index" << index;
        return ;
    }

    elements[index]->bindQuickItem(item);
}

std::shared_ptr<UIBridgeElement> UIBridge::findBySetting(const Setting<QString> &setting) const
{
    for (const std::shared_ptr<UIBridgeElement>& element : qAsConst(elements))
    {
        if (!element)
        {
            continue;
        }

        if (element->getSettingString() == &setting)
        {
            return element;
        }
    }

    return nullptr;
}
