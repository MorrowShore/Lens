#include "uibridge.h"
#include "UIBridgeElement.h"

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

    addElement(element);

    return element;
}

std::shared_ptr<UIBridgeElement> UIBridge::addButton(const QString &text, std::function<void ()> invokeCallback)
{
    std::shared_ptr<UIBridgeElement> element = std::make_shared<UIBridgeElement>();

    element->type = UIBridgeElement::Type::Button;

    element->invokeCallback = invokeCallback;
    element->setItemProperty("text", text);

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
        qWarning() << "setting is null";
    }

    addElement(element);

    return element;
}

std::shared_ptr<UIBridgeElement> UIBridge::addSwitch(Setting<bool> *setting, const QString &name, const QString& hint)
{
    std::shared_ptr<UIBridgeElement> element = std::make_shared<UIBridgeElement>();

    element->type = UIBridgeElement::Type::Switch;
    element->settingBool = setting;
    element->setItemProperty("text", name);
    element->setItemProperty("hint", hint);

    if (setting)
    {
        element->setItemProperty("checked", setting->get());
    }

    addElement(element);

    return element;
}

std::shared_ptr<UIBridgeElement> UIBridge::addSlider(Setting<double> *setting, const QString &name, const double minValue, const double maxValue, const bool valueShowAsPercent)
{
    std::shared_ptr<UIBridgeElement> element = std::make_shared<UIBridgeElement>();

    element->type = UIBridgeElement::Type::Slider;
    element->settingDouble = setting;

    element->setItemProperty("name", name);
    element->setItemProperty("from", minValue);
    element->setItemProperty("to", maxValue);

    if (setting)
    {
        element->setItemProperty("defaultValue", setting->getDefaultValue());
        element->setItemProperty("value", setting->get());
    }

    element->setItemProperty("valueShowAsPercent", valueShowAsPercent);

    addElement(element);

    return element;
}

std::shared_ptr<UIBridgeElement> UIBridge::addComboBox(Setting<int> *setting, const QString &name, const QStringList& items)
{
    std::shared_ptr<UIBridgeElement> element = std::make_shared<UIBridgeElement>();

    element->type = UIBridgeElement::Type::ComboBox;
    element->settingInt = setting;

    element->setItemProperty("name", name);
    element->setItemProperty("model", items);

    if (setting)
    {
        element->setItemProperty("currentIndex", setting->get());
    }

    addElement(element);

    return element;
}

void UIBridge::addElement(const std::shared_ptr<UIBridgeElement> &element)
{
    if (!element)
    {
        qCritical() << "element is null";
        return;
    }

    element->updateItemProperties();

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
        qCritical() << "invalid index" << index;
        return -1;
    }

    return elements[index]->getTypeInt();
}

void UIBridge::bindQuickItem(const int index, QQuickItem *item)
{
    if (index < 0 || index >= elements.count())
    {
        qCritical() << "invalid index" << index;
        return ;
    }

    elements[index]->bindQuickItem(item);
}

void UIBridge::declareQml()
{
    qmlRegisterUncreatableType<UIBridge> ("AxelChat.UIBridge", 1, 0, "UIBridge", "Type cannot be created in QML");
    UIBridgeElement::declareQml();
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
